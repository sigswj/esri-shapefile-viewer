#ifndef SHAPEDATA_H
#define SHAPEDATA_H

#include <memory>
#include <string>
#include "../shapelib/shapefil.h"
#include "nsdef.h"
#include "supports.h"

class QPoint;
class QPainter;
class QColor;

class cl::Dataset::ShapeRecordUnique
{
public:
    ShapeRecordUnique() : _raw(nullptr) {}
    ShapeRecordUnique(ShapeDatasetRC const& dataset, int index);

    // The move constructor and the move assignment operator,
    // used for passing an r-value.
    ShapeRecordUnique(ShapeRecordUnique&& rhs);
    ShapeRecordUnique& operator= (ShapeRecordUnique&& rhs);

    ~ShapeRecordUnique();

    SHPObject& operator* () { return *_raw; }
    SHPObject* operator-> () { return _raw; }
    bool operator== (void* that) const { return _raw == that? true : false; }
    bool operator!= (void* that) const { return _raw != that? true : false; }

private:
    SHPObject* _raw;
};

class cl::Dataset::ShapeDatasetRC
{
    friend class ShapeDatasetSptr;

public:
    ShapeDatasetRC(std::string const& path);
    ~ShapeDatasetRC();

    int type () const { return _shpHandle->nShapeType; }
    SHPHandle const& handle() const { return _shpHandle; }
    SHPTree const& tree() const { return *_shpTree; }
    int recordCount() const { return _shpHandle->nRecords;}
    ShapeRecordUnique readRecord(int index) const;
    Bounds const& bounds() const { return _bounds; }
    std::string const& name() const { return _name; }
    std::vector<int> const filterRecords(Bounds const& mapHitBounds) const;

private:
    SHPHandle _shpHandle;
    SHPTree* _shpTree;
    int _refCount;
    std::string _name;
    Bounds _bounds;

    ShapeDatasetRC* addRef();
};

class cl::Dataset::ShapeDatasetSptr
{
public:
    ShapeDatasetSptr() : _raw(nullptr) {}
    ShapeDatasetSptr(std::string const& path);
    ShapeDatasetSptr(ShapeDatasetRC* shapeDataset);

    ShapeDatasetSptr(ShapeDatasetSptr const& rhs);
    ShapeDatasetSptr& operator= (ShapeDatasetSptr const& rhs);

    ~ShapeDatasetSptr();

    ShapeDatasetRC& operator* () { return *_raw; }
    ShapeDatasetRC* operator-> () { return _raw; }
    bool operator== (void* that) const { return _raw == that? true : false; }
    bool operator!= (void* that) const { return _raw != that? true : false; }

private:
    ShapeDatasetRC* _raw;
};

class cl::Graphics::Shape
{
public:
    virtual ~Shape();
    std::string const& name() const;
    int recordCount() const;
    Bounds const& bounds() const;

    // Return the number of records hit according to the index tree.
    virtual int draw(QPainter& painter, GraphicAssistant const& assistant) const = 0;

protected:
    Shape(Dataset::ShapeDatasetSptr ptrDataset);
    std::unique_ptr<ShapePrivate> _private;
};

class cl::Graphics::Point: public Shape
{
public:
    Point(Dataset::ShapeDatasetSptr ptrDataset): Shape(ptrDataset) {}
    virtual ~Point() {}
    virtual int draw(QPainter& painter, GraphicAssistant const& assistant) const;
};

class cl::Graphics::Polyline: public Shape
{
public:
    Polyline(Dataset::ShapeDatasetSptr ptrDataset): Shape(ptrDataset) {}
    virtual ~Polyline() {}
    virtual int draw(QPainter& painter, GraphicAssistant const& assistant) const;
};

class cl::Graphics::Polygon: public Shape
{
public:
    Polygon(Dataset::ShapeDatasetSptr ptrDataset): Shape(ptrDataset) {}
    virtual ~Polygon() {}
    virtual int draw(QPainter& painter, GraphicAssistant const& assistant) const;
};

enum class ShapeProvider { ESRI = 0, AUTODESK, OTHERS };

class cl::DataManagement::ShapeFactory
{
public:
    virtual ~ShapeFactory() {}
    virtual std::shared_ptr<Graphics::Shape> createShape(std::string const& path) const = 0;

protected:
    ShapeFactory() {}
};

class cl::DataManagement::ShapeFactoryESRI: ShapeFactory
{
public:
    virtual ~ShapeFactoryESRI() {}
    virtual std::shared_ptr<Graphics::Shape> createShape(std::string const& path) const;
    static ShapeFactory const& instance();

private:
    ShapeFactoryESRI() {}
    static std::unique_ptr<ShapeFactory> _instance;
};

#endif // SHAPEDATA_H
