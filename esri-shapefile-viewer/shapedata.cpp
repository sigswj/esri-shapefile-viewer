#include "shapedata.h"
#include <QPoint>
#include <QPainter>
#include <QColor>
#include <QFileInfo>
#include "shapemanager.h"

using namespace cl;

class cl::Graphics::Shape::ShapePrivate
{
    friend class Shape;
    friend class Point;
    friend class MultiPartShape;
    friend class Polyline;
    friend class Polygon;

public:
    ~ShapePrivate() {}

private:
    ShapePrivate(Shape& refThis, Dataset::ShapeDatasetShared const& ptrDataset)
        : _refThis(refThis), _ptrDataset(ptrDataset),
          _borderColor(QColor::fromHsl(qrand()%360, qrand()%256, qrand()%200)),
          _fillColor(QColor::fromHsl(qrand()%360, qrand()%256, qrand()%256)) {}

    Shape& _refThis;

    Dataset::ShapeDatasetShared _ptrDataset;
    QColor _borderColor, _fillColor; // Each object has a different but fixed color set.
};

class cl::Dataset::ShapeDatasetShared::ShapeDatasetRC
{
    friend class ShapeDatasetShared;

public:
    ShapeDatasetRC(std::string const& path);
    ~ShapeDatasetRC();

    int type () const { return _shpHandle->nShapeType; }
    SHPHandle const& handle() const { return _shpHandle; }
    SHPTree const& tree() const { return *_shpTree; }
    int recordCount() const { return _shpHandle->nRecords;}
    Rect<double> const& bounds() const { return _bounds; }
    std::string const& name() const { return _name; }
    std::vector<int> const filterRecords(Rect<double> const& mapHitBounds) const;

private:
    SHPHandle _shpHandle;
    SHPTree* _shpTree;
    int _refCount;
    std::string _name;
    Rect<double> _bounds;

    ShapeDatasetRC* addRef();
};

// Defined here to ensure the unique pointer of ShapePrivate to be destructed properly.
Graphics::Shape::~Shape() {}

std::string const& Graphics::Shape::name() const
{
    return _private->_ptrDataset->name();
}

Graphics::Shape::Shape(Dataset::ShapeDatasetShared const& ptrDataset)
    : _private(std::unique_ptr<ShapePrivate>
               (new ShapePrivate(*this, ptrDataset))) {}

std::unique_ptr<DataManagement::ShapeFactory> DataManagement::ShapeFactoryEsri::_instance = nullptr;

DataManagement::ShapeFactory const& DataManagement::ShapeFactoryEsri::instance()
{
    if (_instance == nullptr)
        _instance.reset(new ShapeFactoryEsri());
    return *_instance;
}

std::shared_ptr<Graphics::Shape> DataManagement::ShapeFactoryEsri::createShape(std::string const& path) const
{
    Dataset::ShapeDatasetShared ptrDataset(path);
    switch (ptrDataset->type())
    {
    case SHPT_POINT:
    case SHPT_POINTZ:
    case SHPT_POINTM:
        return std::shared_ptr<Graphics::Shape>(new Graphics::Point(ptrDataset));
        break;

    case SHPT_ARC:
    case SHPT_ARCZ:
    case SHPT_ARCM:
        return std::shared_ptr<Graphics::Shape>(new Graphics::Polyline(ptrDataset));
        break;

    case SHPT_POLYGON:
    case SHPT_POLYGONZ:
    case SHPT_POLYGONM:
        return std::shared_ptr<Graphics::Shape>(new Graphics::Polygon(ptrDataset));
        break;
    default:
        return nullptr;
        break;
    }
}

int Graphics::Point::draw(QPainter& painter, GraphicAssistant const& assistant) const
{
    Rect<double> mapHitBounds = assistant.computeMapHitBounds();
    std::vector<int> recordsHit = _private->_ptrDataset->filterRecords(mapHitBounds);

    painter.setPen(QPen(_private->_borderColor));
    painter.setBrush(QBrush(_private->_fillColor));

    for (auto item : recordsHit)
    {
        Dataset::ShapeRecordUnique ptrRecord = _private->_ptrDataset.readRecord(item);
        QPoint point = assistant.computePointOnDisplay(*ptrRecord, 0).toQPoint();

        int const r = 5;

        painter.drawEllipse(point, r, r);
    }

    return recordsHit.size();
}

int Graphics::MultiPartShape::draw(QPainter& painter, GraphicAssistant const& assistant) const
{
    Rect<double> mapHitBounds = assistant.computeMapHitBounds();
    std::vector<int> recordsHit = _private->_ptrDataset->filterRecords(mapHitBounds);

    painter.setPen(QPen(_private->_borderColor));
    painter.setBrush(QBrush(_private->_fillColor));

    for (auto item : recordsHit)
    {
        Dataset::ShapeRecordUnique ptrRecord = _private->_ptrDataset.readRecord(item);
        ptrRecord->panPartStart[ptrRecord->nParts] = ptrRecord->nVertices;

        for (int partIndex = 0; partIndex < ptrRecord->nParts; ++partIndex)
        {
            int nPartVertices = ptrRecord->panPartStart[partIndex + 1] - ptrRecord->panPartStart[partIndex];
            QPoint partVertices[nPartVertices];

            int count = 0;
            for (int vtxIndex = ptrRecord->panPartStart[partIndex]; vtxIndex < ptrRecord->panPartStart[partIndex + 1]; ++vtxIndex)
                partVertices[count++] = assistant.computePointOnDisplay(*ptrRecord, vtxIndex).toQPoint();

            drawPart(painter, partVertices, nPartVertices);
        }
    }

    return recordsHit.size();
}

void Graphics::Polyline::drawPart(QPainter& painter, QPoint const* points, int pointCount) const
{
    painter.drawPolyline(points, pointCount);
}

void Graphics::Polygon::drawPart(QPainter& painter, QPoint const* points, int pointCount) const
{
    painter.drawPolyline(points, pointCount);
}


int Graphics::Shape::recordCount() const
{
    return _private->_ptrDataset->recordCount();
}

Dataset::ShapeDatasetShared::ShapeDatasetRC::ShapeDatasetRC(std::string const& path)
    : _shpHandle(nullptr), _shpTree(nullptr), _refCount(1)
{
    _shpHandle = SHPOpen(path.c_str(), "rb+");
    _shpTree = SHPCreateTree(_shpHandle, 2, 10, nullptr, nullptr);
    SHPTreeTrimExtraNodes(_shpTree);

    QFileInfo fileInfo(QString::fromStdString(path));
    _name = fileInfo.baseName().toStdString();

    _bounds = Rect<double>(_shpHandle->adBoundsMin, _shpHandle->adBoundsMax);
}

Rect<double> const& Graphics::Shape::bounds() const
{
    return _private->_ptrDataset->bounds();
}
Dataset::ShapeDatasetShared::ShapeDatasetRC::~ShapeDatasetRC()
{
    if(_shpHandle)
    {
        SHPClose(_shpHandle);
        _shpHandle = nullptr;
    }

    if(_shpTree)
    {
        SHPDestroyTree(_shpTree);
        _shpTree = nullptr;
    }
}

Dataset::ShapeDatasetShared::ShapeDatasetRC* Dataset::ShapeDatasetShared::ShapeDatasetRC::addRef()
{
    ++_refCount;
    return this;
}

Dataset::ShapeDatasetShared::ShapeDatasetShared(std::string const& path)
{
    _raw = new ShapeDatasetRC(path);
}

Dataset::ShapeDatasetShared::ShapeDatasetShared(ShapeDatasetRC* shapeDataset)
    : _raw(shapeDataset) {}


Dataset::ShapeDatasetShared::ShapeDatasetShared(ShapeDatasetShared const& rhs)
    : _raw(rhs._raw->addRef()) {}

Dataset::ShapeDatasetShared& Dataset::ShapeDatasetShared::operator= (ShapeDatasetShared const& rhs)
{
    if(this == &rhs)
        return *this;

    if(--_raw->_refCount == 0)
        delete _raw;

    _raw = rhs._raw->addRef();

    return *this;
}

//ShapeDatasetShared::ShapeDatasetShared(ShapeDatasetShared const& rhs)
//    : _raw(rhs._raw)
//{
//    ++_raw->_refCount;
//}

//ShapeDatasetShared& ShapeDatasetShared::operator= (ShapeDatasetShared const& rhs)
//{
//    if(this == &rhs)
//        return *this;

//    if(--_raw->_refCount == 0)
//        delete _raw;

//    _raw = rhs._raw;
//    ++_raw->_refCount;

//    return *this;
//}

Dataset::ShapeDatasetShared::~ShapeDatasetShared()
{
    if(_raw && --_raw->_refCount == 0)
        delete _raw;
}

std::vector<int> const Dataset::ShapeDatasetShared::ShapeDatasetRC::filterRecords(Rect<double> const& mapHitBounds) const
{
    double mapHitBoundsMin[2] = {mapHitBounds.xMin(), mapHitBounds.yMin()};
    double mapHitBoundsMax[2] = {mapHitBounds.xMax(), mapHitBounds.yMax()};

    int hitCount;
    int* recordsHitArray = SHPTreeFindLikelyShapes(_shpTree, mapHitBoundsMin, mapHitBoundsMax, &hitCount);

    std::vector<int> recordsHit;
    for (int i = 0; i < hitCount; ++i)
        recordsHit.push_back(recordsHitArray[i]);

    return recordsHit;
}

Dataset::ShapeRecordUnique::~ShapeRecordUnique()
{
    if(_raw)
        SHPDestroyObject(_raw);
}

Dataset::ShapeRecordUnique::ShapeRecordUnique(ShapeDatasetShared const& ptrDataset, int index)
    : _raw(SHPReadObject(ptrDataset->handle(), index)) {}

Dataset::ShapeRecordUnique::ShapeRecordUnique(ShapeRecordUnique&& rhs)
{
    _raw = rhs._raw;
    rhs._raw = nullptr;
}

Dataset::ShapeRecordUnique& Dataset::ShapeRecordUnique::operator= (ShapeRecordUnique&& rhs)
{
    _raw = rhs._raw;
    rhs._raw = nullptr;
    return *this;
}

Dataset::ShapeRecordUnique Dataset::ShapeDatasetShared::readRecord(int index) const
{
    return ShapeRecordUnique(*this, index);
}
