#include "shapedata.h"
#include <QPoint>
#include <QPainter>
#include <QColor>
#include <QFileInfo>
#include "shapemanager.h"

using namespace cl;

class cl::Graphics::ShapePrivate
{
    friend class Shape;
    friend class Point;
    friend class Polyline;
    friend class Polygon;

public:
    ~ShapePrivate() {}

private:
    ShapePrivate(Shape& refThis, Dataset::ShapeDatasetSptr ptrDataset)
        : _refThis(refThis), _ptrDataset(ptrDataset),
          _borderColor(QColor::fromHsl(qrand()%360, qrand()%256, qrand()%200)),
          _fillColor(QColor::fromHsl(qrand()%360, qrand()%256, qrand()%256)) {}

    Shape& _refThis;

    Dataset::ShapeDatasetSptr _ptrDataset;
    QColor _borderColor, _fillColor; // Each object has a different but fixed color set.
};

Graphics::Shape::~Shape() {}

std::string const& Graphics::Shape::name() const
{
    return _private->_ptrDataset->name();
}

Graphics::Shape::Shape(Dataset::ShapeDatasetSptr ptrDataset)
    : _private(std::unique_ptr<ShapePrivate>
               (new ShapePrivate(*this, ptrDataset))) {}

std::unique_ptr<DataManagement::ShapeFactory> DataManagement::ShapeFactoryESRI::_instance = nullptr;

DataManagement::ShapeFactory const& DataManagement::ShapeFactoryESRI::instance()
{
    if (_instance == nullptr)
        _instance.reset(new ShapeFactoryESRI());
    return *_instance;
}

std::shared_ptr<Graphics::Shape> DataManagement::ShapeFactoryESRI::createShape(std::string const& path) const
{
    Dataset::ShapeDatasetSptr ptrDataset(path);
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
    Bounds mapHitBounds = assistant.computeMapHitBounds();
    std::vector<int> recordsHit = _private->_ptrDataset->filterRecords(mapHitBounds);

    painter.setPen(QPen(_private->_borderColor));
    painter.setBrush(QBrush(_private->_fillColor));

    for (auto item : recordsHit)
    {
        Dataset::ShapeRecordUnique ptrRecord = _private->_ptrDataset->readRecord(item);
        QPoint point = assistant.computePointOnDisplay(*ptrRecord, 0);

        int const r = 5;

        painter.drawEllipse(point, r, r);
    }

    return recordsHit.size();
}

int Graphics::Polyline::draw(QPainter& painter, GraphicAssistant const& assistant) const
{
    Bounds mapHitBounds = assistant.computeMapHitBounds();
    std::vector<int> recordsHit = _private->_ptrDataset->filterRecords(mapHitBounds);

    painter.setPen(QPen(_private->_borderColor));
    painter.setBrush(QBrush(_private->_fillColor));

    for (auto item : recordsHit)
    {
        Dataset::ShapeRecordUnique ptrRecord = _private->_ptrDataset->readRecord(item);
        ptrRecord->panPartStart[ptrRecord->nParts] = ptrRecord->nVertices;

        for (int partIndex = 0; partIndex < ptrRecord->nParts; ++partIndex)
        {
            int nPartVertices = ptrRecord->panPartStart[partIndex+1] - ptrRecord->panPartStart[partIndex];
            QPoint partVertices[nPartVertices];

            int count = 0;
            for (int vtxIndex = ptrRecord->panPartStart[partIndex]; vtxIndex < ptrRecord->panPartStart[partIndex+1]; ++vtxIndex)
                partVertices[count++] = assistant.computePointOnDisplay(*ptrRecord, vtxIndex);

            painter.drawPolyline(partVertices, nPartVertices);
        }
    }

    return recordsHit.size();
}

int Graphics::Polygon::draw(QPainter& painter, GraphicAssistant const& assistant) const
{
    Bounds mapHitBounds = assistant.computeMapHitBounds();
    std::vector<int> recordsHit = _private->_ptrDataset->filterRecords(mapHitBounds);

    painter.setPen(QPen(_private->_borderColor));
    painter.setBrush(QBrush(_private->_fillColor));

    for (auto item : recordsHit)
    {
        Dataset::ShapeRecordUnique ptrRecord = _private->_ptrDataset->readRecord(item);
        ptrRecord->panPartStart[ptrRecord->nParts] = ptrRecord->nVertices;

        for (int partIndex = 0; partIndex < ptrRecord->nParts; ++partIndex)
        {
            int nPartVertices = ptrRecord->panPartStart[partIndex+1] - ptrRecord->panPartStart[partIndex];
            QPoint partVertices[nPartVertices];

            int count = 0;
            for (int vtxIndex = ptrRecord->panPartStart[partIndex]; vtxIndex < ptrRecord->panPartStart[partIndex+1]; ++vtxIndex)
                partVertices[count++] = assistant.computePointOnDisplay(*ptrRecord, vtxIndex);

            painter.drawPolygon(partVertices, nPartVertices);
        }
    }

    return recordsHit.size();
}

int Graphics::Shape::recordCount() const
{
    return _private->_ptrDataset->recordCount();
}

Dataset::ShapeDatasetRC::ShapeDatasetRC(std::string const& path)
    : _shpHandle(nullptr), _shpTree(nullptr), _refCount(1)
{
    _shpHandle = SHPOpen(path.c_str(), "rb+");
    _shpTree = SHPCreateTree(_shpHandle, 2, 10, nullptr, nullptr);
    SHPTreeTrimExtraNodes(_shpTree);

    QFileInfo fileInfo(QString::fromStdString(path));
    _name = fileInfo.baseName().toStdString();

    _bounds.set(_shpHandle->adBoundsMin, _shpHandle->adBoundsMax);
}

Bounds const& Graphics::Shape::bounds() const
{
    return _private->_ptrDataset->bounds();
}
Dataset::ShapeDatasetRC::~ShapeDatasetRC()
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

Dataset::ShapeDatasetRC* Dataset::ShapeDatasetRC::addRef()
{
    ++_refCount;
    return this;
}

Dataset::ShapeDatasetSptr::ShapeDatasetSptr(std::string const& path)
{
    _raw = new ShapeDatasetRC(path);
}

Dataset::ShapeDatasetSptr::ShapeDatasetSptr(ShapeDatasetRC* shapeDataset)
    : _raw(shapeDataset) {}


Dataset::ShapeDatasetSptr::ShapeDatasetSptr(ShapeDatasetSptr const& rhs)
    : _raw(rhs._raw->addRef()) {}

Dataset::ShapeDatasetSptr& Dataset::ShapeDatasetSptr::operator= (ShapeDatasetSptr const& rhs)
{
    if(this == &rhs)
        return *this;

    if(--_raw->_refCount == 0)
        delete _raw;

    _raw = rhs._raw->addRef();

    return *this;
}

//ShapeDatasetSptr::ShapeDatasetSptr(ShapeDatasetSptr const& rhs)
//    : _raw(rhs._raw)
//{
//    ++_raw->_refCount;
//}

//ShapeDatasetSptr& ShapeDatasetSptr::operator= (ShapeDatasetSptr const& rhs)
//{
//    if(this == &rhs)
//        return *this;

//    if(--_raw->_refCount == 0)
//        delete _raw;

//    _raw = rhs._raw;
//    ++_raw->_refCount;

//    return *this;
//}

Dataset::ShapeDatasetSptr::~ShapeDatasetSptr()
{
    if(_raw && --_raw->_refCount == 0)
        delete _raw;
}

std::vector<int> const Dataset::ShapeDatasetRC::filterRecords(Bounds const& mapHitBounds) const
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

Dataset::ShapeRecordUnique::ShapeRecordUnique(ShapeDatasetRC const& dataset, int index)
    : _raw(SHPReadObject(dataset.handle(), index)) {}

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

Dataset::ShapeRecordUnique Dataset::ShapeDatasetRC::readRecord(int index) const
{
    return ShapeRecordUnique(*this, index);
}
