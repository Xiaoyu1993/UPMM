/*
    This file is part of LibImportance library that provides a technique for guiding
    transport paths towards the important places in the scene. This is a direct implementation
    of the method described in the paper "On-line Learning of Parametric Mixture 
    Models for Light Transport Simulation", ACM Trans. Graph. (SIGGRAPH 2014) 33, 4 (2014).
   
    Copyright (c) 2014 by Jiri Vorba, Ondrej Karlik, Martin Sik.

    LibImportance library is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    LibImportance library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once
#include "..\shared\Config.h"
#include "ReaderWriterLock.h"

namespace Importance {

    template<class TDistributionModel> class ImportanceCache;

    template<class TDistributionModel>
    class InterpolationResult : public Distribution {
    public:

        virtual ~InterpolationResult() { }
        virtual int numRecordsUsed() const = 0;
        virtual const TDistributionModel* getModel(const int i) const = 0;
    };

template<class TDistributionModel>
class CrossfadeInterpolationResult : public InterpolationResult<TDistributionModel> {
public:

    ImStaticArray<const TDistributionModel*, MAX_CACHE_KNN> records;
    ImStaticArray<float,         MAX_CACHE_KNN> cdf;
    int found;

    IMPORTANCE_INLINE float pdf(const Vector3f _dir) const {
        const Vector3 direction(_dir);
        float result = 0.f;
        float previousWeight = 0.f;
        for(int i = 0; i < found; i++) {
            result += float(records[i]->pdf(direction))*(cdf[i]-previousWeight);
            previousWeight = cdf[i];
        }
        return result / cdf[found-1];
    }

    IMPORTANCE_INLINE float getWeight(const int index) const {
        return (index == 0) ? cdf[0] : (cdf[index]-cdf[index-1]);
    }

    int numRecordsUsed() const {
        return found;
    }
    const TDistributionModel* getModel(const int i) const {
        return records[i];
    }

    IMPORTANCE_INLINE CrossfadeInterpolationResult() { }


    virtual const EmInfo * getInfo() const {
        IMPORTANCE_ASSERT( found == 1 );
        return records[ 0 ]->getInfo();
    }

    explicit IMPORTANCE_INLINE CrossfadeInterpolationResult(const TDistributionModel* record) {
        found = 1;
        records[0] = record;
        cdf[0] = 1.f;
    }

    virtual void pdfs(const Vector3 * directions, float* pdfs, const int count) const {
        for(int i = 0; i < count; i++) {
            pdfs[i] = this->pdf(directions[i]);
        }
    }

    virtual void sampleDirections(const Vector2* random, Vector3 * output, float* pdfs, const int count) {
        for(int i = 0; i < count; i++) {
            Vector2 newRandom = random[i];
            const int index = discreteSelection(newRandom.y, cdf.ptr(), cdf.ptr()+found);
            output[i] = records[index]->sampleDirection(newRandom).singlePrecision();
            IMPORTANCE_ASSERT(output[i].isReal());
            pdfs[i] = this->pdf(output[i]);
            IMPORTANCE_ASSERT(pdfs[i] >= 0.f);
        }
    }

    virtual void release() {}

    virtual std::string toString() const {
        std::ostringstream ostr;
        ostr << "CacheInterpolationResult(Distribution)[ " << std::endl;
        for ( int i = 0; i < found; i++ ) {
            ostr << records[ i ]->toString() << std::endl;
        }
        ostr << " ]";
        return ostr.str();
    }

    //////////////////////////////////////////////////////////////////////////
    // InterpolationResult interface
    //////////////////////////////////////////////////////////////////////////

    float initResult(const ImportanceCache<TDistributionModel> & cache, KdQueryResult* results, const int used, const Hit&) {
        this->found = used;
        cache.fillRecords( records, results, used );

        for(int i = 0; i < used; ++i) {
            IMPORTANCE_ASSERT(results[i].distSqr > 0.f);
            cdf[i] = (i==0) ? results[i].distSqr : (results[i].distSqr+cdf[i-1]);
        }
        IMPORTANCE_ASSERT(used == 0 || cdf[used-1] > 0.f);
        return 0.f;
    }
};

template<class TDistributionModel>
struct CacheRecord {
private:
    CacheRecord(const CacheRecord&)  { }
    CacheRecord& operator=(const CacheRecord&) {
        return this;
    }
protected:
    IMPORTANCE_INLINE CacheRecord() { }
    Float _validRadius;    
public:

    typename TDistributionModel distr;   

    Vector3 position;      // position of the record in the world space
    Vector3 normal;
    float gradient; //DEBUG
	float originalRadius;

    mutable ReaderWriterLock lock;

    ~CacheRecord() {
        distr.release();
    }

    IMPORTANCE_INLINE void setValidRadius(const float radius) {
        this->_validRadius = radius;
    }
    IMPORTANCE_INLINE Float validRadius() const {
        return _validRadius;
    }

    IMPORTANCE_INLINE CacheRecord(const Vector3 position, const Vector3 normal, const Float radius) 
        : position(position), normal(normal), _validRadius(radius) { }
};


}