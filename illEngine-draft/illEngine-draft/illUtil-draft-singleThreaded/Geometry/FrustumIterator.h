#ifndef __FRUSTUM_ITERATOR_H__
#define __FRUSTUM_ITERATOR_H__

#include <stdexcept>
#include <list>

#include "../StaticList.h"
#include "GridVolume3D.h"
#include "Frustum.h"
#include "geomUtil.h"

const bool LEFT_SIDE = true;
const bool RIGHT_SIDE = false;

/**
Traverses front to back in the view frustum that intersects a GridVolume3D.
The frustum should have a field of view less than 180 degrees or else assumptions made about
the traversal will be wrong.

@param W The precision of the world space
@param P The precision of the 3d grid volume cell subdivision
*/
template <typename W = glm::mediump_float, typename P = int>
class FrustumIterator {
public:
    struct Debugger {
        Debugger() {
            for(uint8_t edge = 0; edge < FRUSTUM_NUM_EDGES; edge++) {
                m_discarededEdges[edge] = false;
            }
        }

        /**
        A little convenience point to turn those 2D slice points into a 3D point for rendering
        */
        glm::detail::tvec3<W> sliceTo3D(const glm::detail::tvec2<W>& coord, W sliceCoord) const {
            glm::detail::tvec3<W> res;

            res[m_iterator->m_dimensionOrder[0]] = sliceCoord;
            res[m_iterator->m_dimensionOrder[1]] = coord.y;
            res[m_iterator->m_dimensionOrder[2]] = coord.x;

            return res;
        }

        FrustumIterator * m_iterator;
        Frustum<W> m_frustum;

        bool m_discarededEdges[FRUSTUM_NUM_EDGES];

        StaticList<W, FRUSTUM_NUM_EDGES> m_pointListMissingDim[2];  //the missing 3rd dimension from the point after clipping edge against plane, used in the debug draw

        std::list<glm::detail::tvec2<W>*> m_sortedSlicePoints;
        std::list<glm::detail::tvec2<W> > m_clipPoints;    //adds points that were clipped during the clipping phase so I can see the results

        StaticList<glm::detail::tvec2<W>*, FRUSTUM_NUM_EDGES> m_unclippedRasterizeEdges[2];

        std::list<glm::detail::tvec3<W> > m_rasterizedCells;  //cells that were rasterized so far in world coords for the renderer to render

        glm::detail::tvec2<P> m_sliceMin;
    };

    FrustumIterator() {
    }

    FrustumIterator(const Frustum<W>* frustum, const Box<P>& range, const glm::detail::tvec3<W>& cellDimensions)
        : m_frustum(frustum),
        m_range(range),
        m_cellDimensions(cellDimensions)
    {
        //initialize edges lists
        memset(m_inactiveEdges, 1, sizeof(bool) * FRUSTUM_NUM_EDGES);
        memset(m_activeEdges, 0, sizeof(int) * FRUSTUM_NUM_EDGES);

        //initialize a bunch of things
        m_dimensionOrder = sortDimensions(m_frustum->m_direction);
        m_directionSign = glm::sign(m_frustum->m_direction);

        //reorder the bounds box according to the direction sign
        for(uint8_t dimension = 0; dimension < 3; dimension++) {
            if(m_directionSign[dimension] < 0) {
                P temp = m_range.m_min[dimension];
                m_range.m_min[dimension] = m_range.m_max[dimension];
                m_range.m_max[dimension] = temp;
            }
        }

        m_debugger.m_iterator = this;
        m_debugger.m_frustum = Frustum<W>(*m_frustum);
        //make iterator itself refer to debugger copy so as I'm looking around it doesn't change
        m_frustum = &m_debugger.m_frustum;

        m_spaceRange = Box<W>(m_cellDimensions * vec3cast<P, W>(m_range.m_min), m_cellDimensions * vec3cast<P, W>(m_range.m_max));

        m_currentPosition = m_range.m_min;

        uint8_t sliceDimension = m_dimensionOrder[0];

        m_sliceStart = m_spaceRange.m_min[sliceDimension];
        W sliceEnd = m_sliceStart + m_directionSign[sliceDimension] * m_cellDimensions[sliceDimension];

        //set up the slice plane, it's normal is in the direction of the slice dimension
        m_slicePlane.m_normal = glm::detail::tvec3<W>((W) 0);
        m_slicePlane.m_normal[sliceDimension] = m_directionSign[sliceDimension];
        m_slicePlane.m_distance = -m_directionSign[sliceDimension] * sliceEnd;

        m_currentPointList = false;

        //find points within first slice
        bool debugFoundPoints = false;   //TODO: remove this, normally points will always be found once I'm done developing

        for(uint8_t point = 0; point < FRUSTUM_NUM_POINTS; point++) {
            if((m_directionSign[sliceDimension] > 0 && m_frustum->m_points[point][sliceDimension] < sliceEnd)
                || (m_directionSign[sliceDimension] < 0 && m_frustum->m_points[point][sliceDimension] >= sliceEnd)) {

                    debugFoundPoints = true;         //TODO: remove this, normally points will always be found once I'm done developing
                    addPoint(point, m_activeEdges);
            }
        }

        if(!debugFoundPoints) { //TODO: remove this, normally points will always be found once I'm done developing
            return;
        }

        if(!setupSlice()) {
            while(!advanceSlice()) {}
        }
    }

    ~FrustumIterator() {
    }

    inline bool atEnd() const {
        return m_currentPosition[m_dimensionOrder[2]] == m_sliceMax.x 
            && m_currentPosition[m_dimensionOrder[1]] == m_sliceMax.y 
            && geq(m_currentPosition[m_dimensionOrder[0]], m_range.m_max[m_dimensionOrder[0]], m_directionSign[0]);
    }

    inline bool forward() {      
        if(m_currentPosition[m_dimensionOrder[2]] == m_sliceMax.x) {
            if(m_currentPosition[m_dimensionOrder[1]] == m_sliceMax.y) {
                while(!advanceSlice()) {
                    return atEnd();
                }
            }
            else {
                advanceRow();
            }         
        }
        else {
            m_currentPosition[m_dimensionOrder[2]] += m_directionSign[m_dimensionOrder[2]];

            m_debugger.m_rasterizedCells.push_back(m_cellDimensions * vec3cast<P, W>(m_currentPosition) + m_cellDimensions * 0.5f * vec3cast<int8_t, W>(m_directionSign));
        }

        return true;
    }

    inline const glm::detail::tvec3<P>& getCurrentPosition() const {
        if(atEnd()) {
            throw std::runtime_error("calling getCurrentPosition() on frustum iterator when at end");
        }

        return m_currentPosition;
    }

    //TODO: temporarily public while developing
    //private:   
    /**
    finds an inactive edge given a point, and removes it from the inactive edge table

    @param pointIndex A point for the edge being looked for
    @param startEdgeIndex Which edge index to start looking at in the inactive edge list
    @param otherPointDestination The destination of where to write the index of the other point that makes up the edge

    @return Either the edge index for the point, or FRUSTUM_NUM_EDGES
    */
    uint8_t findInactiveEdge(uint8_t pointIndex, uint8_t startEdgeIndex, bool& otherPointDestination) {
        for(; startEdgeIndex < FRUSTUM_NUM_EDGES; startEdgeIndex++) {
            if(m_inactiveEdges[startEdgeIndex]) {
                if(FRUSTUM_EDGE_LIST[startEdgeIndex][0] == pointIndex) {
                    m_inactiveEdges[startEdgeIndex] = false;
                    otherPointDestination = 1;
                    return startEdgeIndex;
                }

                if(FRUSTUM_EDGE_LIST[startEdgeIndex][1] == pointIndex) {
                    m_inactiveEdges[startEdgeIndex] = false;
                    otherPointDestination = 0;
                    return startEdgeIndex;
                }
            }
        }

        return FRUSTUM_NUM_EDGES;
    }

    /**
    Returns how many grid squares are spanned by some world distance in some dimension.

    @param worldDistance The distance.  It's possible to use a negative distance to get the negative number of cells.  The distance is negated by the directionSign of the specified dimension as well.
    @param dimension The x, y, or z dimension.  Use 0, 1, 2 to index into it.
    */
    inline int gridDistance(W worldDistance, uint8_t dimension) {
        return (int) glm::floor(worldDistance * m_directionSign[dimension] / m_cellDimensions[dimension]);
    }

    /**
    Adds a point from the 3D polygon being rasterized.
    */
    void addPoint(uint8_t point, int* activeEdgesDestination) {
        uint8_t inactiveEdgeIndex = 0;
        bool otherPoint;

        m_pointList[m_currentPointList].add(glm::detail::tvec2<W>(m_frustum->m_points[point][m_dimensionOrder[2]], m_frustum->m_points[point][m_dimensionOrder[1]]));
        m_debugger.m_pointListMissingDim[m_currentPointList].add(m_frustum->m_points[point][m_dimensionOrder[0]]);

        while((inactiveEdgeIndex = findInactiveEdge(point, inactiveEdgeIndex, otherPoint)) != FRUSTUM_NUM_EDGES) {
            //find which slice the other point is in relative to this slice
            int sliceNum = gridDistance(m_frustum->m_points[FRUSTUM_EDGE_LIST[inactiveEdgeIndex][otherPoint]][m_dimensionOrder[0]] - m_sliceStart, m_dimensionOrder[0]);

            //discard edge if also in this slice
            if(sliceNum <= 0) {
                m_debugger.m_discarededEdges[inactiveEdgeIndex] = true;
            }
            else {
                //add edge to active edges
                activeEdgesDestination[inactiveEdgeIndex] = sliceNum;
                m_activeEdgeDestPoint[inactiveEdgeIndex] = otherPoint;
            }

            inactiveEdgeIndex++; //advance for next inactive edge search
        }
    }

    /**
    Adds a point from the slice being rasterized.

    @tparam isLeftSide Whether or not being called on the right side or the left side of the rasterizing polygon
    */
    template <bool isLeftSide>
    inline void addSlicePoint() {
        //TODO: rewriting this farthestColumn stuff

        int farthestColumn = 0;

        bool foundThisRow = false;

        while(true) {
            //find which row the other point is in relative to this row
            glm::detail::tvec2<W>& pointA = m_sliceRasterizeEdges[isLeftSide].m_data[m_activeSliceEdgeIndex[isLeftSide]];
            glm::detail::tvec2<W>& pointB = m_sliceRasterizeEdges[isLeftSide].m_data[m_activeSliceEdgeIndex[isLeftSide] + 1];

            int rowNum = gridDistance((pointB.y - pointA.y) * m_directionSign[m_dimensionOrder[1]], m_dimensionOrder[1]);

            int column = gridDistance(isLeftSide
                ? (m_spaceRange.m_max[m_dimensionOrder[2]] - pointB.x) * m_directionSign[m_dimensionOrder[2]]
            : (pointB.x - m_spaceRange.m_min[m_dimensionOrder[2]]) * m_directionSign[m_dimensionOrder[2]], 
                m_dimensionOrder[2]);

            //if next point is in the same row, advance
            if(rowNum <= 0) {
                if(column > farthestColumn) {
                    farthestColumn = column;
                }

                foundThisRow = true;

                if(m_activeSliceEdgeIndex[isLeftSide] + 1 < m_sliceRasterizeEdges[isLeftSide].m_size - 1) {
                    m_activeSliceEdgeIndex[isLeftSide]++;
                }
                else {
                    m_acticeSliceEdgeOutward[isLeftSide] = gt(pointB.x, pointA.x, isLeftSide ? -m_directionSign[m_dimensionOrder[2]] : m_directionSign[m_dimensionOrder[2]]);
                    m_activeSliceEdges[isLeftSide] = rowNum;

                    break;
                }
            }
            else {
                m_acticeSliceEdgeOutward[isLeftSide] = gt(pointB.x, pointA.x, isLeftSide ? -m_directionSign[m_dimensionOrder[2]] : m_directionSign[m_dimensionOrder[2]]);
                m_activeSliceEdges[isLeftSide] = rowNum;
                break;
            }
        }

        if(foundThisRow) {
            if(isLeftSide) {
                m_currentPosition[m_dimensionOrder[2]] = m_range.m_max[m_dimensionOrder[2]] - farthestColumn * m_directionSign[m_dimensionOrder[2]];
                m_debugger.m_sliceMin.x = m_currentPosition[m_dimensionOrder[2]];
            }
            else {
                m_sliceMax.x = m_range.m_min[m_dimensionOrder[2]] + farthestColumn * m_directionSign[m_dimensionOrder[2]];
            }
        }
    }

    struct PointComparator {
        inline PointComparator(FrustumIterator& frustumIterator)
            : m_frustumIterator(frustumIterator)
        {}

        inline bool operator() (glm::detail::tvec2<W>* ptA, glm::detail::tvec2<W>* ptB) {
            for(uint8_t dimension = 1; dimension != 0; dimension = 0) {
                if(m_frustumIterator.m_directionSign[m_frustumIterator.m_dimensionOrder[dimension]] > 0) {
                    if((*ptA)[dimension] < (*ptB)[dimension]) {
                        return true;
                    }
                }
                else {
                    if((*ptA)[dimension] > (*ptB)[dimension]) {
                        return true;
                    }
                }
            }

            return false;
        }

        FrustumIterator& m_frustumIterator;
    };

    template <typename Iter>
    static void convexHull(Iter& iter, Iter& end, StaticList<glm::detail::tvec2<W>*, FRUSTUM_NUM_EDGES>& hullEdges, int8_t sign) {                  
        if(iter != end) {
            //init some things first
            glm::detail::tvec2<W>* point = *iter;            
            hullEdges.add(point);
            iter++;

            if(iter != end) {
                point = *iter;
                hullEdges.add(point);
                iter++;

                //now do the actual loop
                for(; iter != end; iter++) {
                    point = *iter;

                    while(hullEdges.m_size >= 2 && leq(cross(*hullEdges.m_data[hullEdges.m_size - 2] - *point, *hullEdges.m_data[hullEdges.m_size - 1] - * point), (W) 0, sign)) {
                        hullEdges.m_size--;
                    }

                    hullEdges.add(point);
                }
            }
        }
    }

    template <bool isLeftSide>
    inline void clipHull(const StaticList<glm::detail::tvec2<W>*, FRUSTUM_NUM_EDGES>& unclippedRasterizeEdges, glm::detail::tvec2<W> min, glm::detail::tvec2<W> max, int8_t xSign) {
        int8_t currentPointIndex = isLeftSide
            ? unclippedRasterizeEdges.m_size - 1
            : 0;

        glm::detail::tvec2<W> prevPoint;
        glm::detail::tvec2<W> currentPoint;

        enum PointRegion {
            PR_OUTSIDE_MIN,
            PR_INSIDE,
            PR_OUTSIDE_MAX,

            PR_UNINITIALIZED
        };

        PointRegion prevPointRegion = PR_UNINITIALIZED;
        PointRegion currentPointRegion = PR_UNINITIALIZED;

        //find the first line segment inside the vertical region
        for(bool clipped = false; isLeftSide ? currentPointIndex >= 0 : currentPointIndex < unclippedRasterizeEdges.m_size; currentPointIndex += isLeftSide ? -1 : 1) {
            currentPoint = *unclippedRasterizeEdges.m_data[currentPointIndex];

            //if past the min vertical bounds
            if(geq(currentPoint.y, min.y, m_directionSign[m_dimensionOrder[1]])) {
                //find the current point's side region
                if(geq(currentPoint.x, min.x, xSign)) {
                    if(leq(currentPoint.x, max.x, xSign)) {
                        currentPointRegion = PR_INSIDE;
                    }
                    else {
                        currentPointRegion = PR_OUTSIDE_MAX;
                    }
                }
                else {
                    currentPointRegion = PR_OUTSIDE_MIN;
                }

                if(clipped) {
                    //clip segment against min vert bounds and set prev point to clipped point            
                    prevPoint.x = lineInterceptX(prevPoint, currentPoint, min.y);
                    prevPoint.y = min.y;

                    m_debugger.m_clipPoints.push_back(prevPoint);

                    //find the previous point's side region
                    if(geq(prevPoint.x, min.x, xSign)) {
                        if(leq(prevPoint.x, max.x, xSign)) {
                            prevPointRegion = PR_INSIDE;
                        }
                        else {
                            prevPointRegion = PR_OUTSIDE_MAX;
                        }
                    }
                    else {
                        prevPointRegion = PR_OUTSIDE_MIN;
                    }

                    if(prevPointRegion == PR_OUTSIDE_MAX) {
                        m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, min.y));
                    }
                    else if(prevPointRegion == PR_OUTSIDE_MIN) {
                        if(currentPointRegion == PR_OUTSIDE_MIN) {
                            //if no more points
                            if(isLeftSide ? currentPointIndex == 0 : currentPointIndex + 1 == unclippedRasterizeEdges.m_size) {
                                //slice is outside the volume
                                assert(m_sliceRasterizeEdges[isLeftSide].m_size == 0);

                                return;
                            }
                        }
                    }
                    else {
                        assert(prevPointRegion == PR_INSIDE);

                        m_sliceRasterizeEdges[isLeftSide].add(prevPoint);
                    }
                }
                else {
                    if(gt(currentPoint.y, max.y, m_directionSign[m_dimensionOrder[1]])) {
                        //slice is outside the volume
                        assert(m_sliceRasterizeEdges[isLeftSide].m_size == 0);

                        return;
                    }

                    //if unclipped and outside max, add the point projected to the side
                    if(currentPointRegion == PR_OUTSIDE_MAX) {
                        m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, currentPoint.y));
                    }
                    else if(currentPointRegion == PR_INSIDE) {
                        m_sliceRasterizeEdges[isLeftSide].add(currentPoint);
                    }

                    prevPoint = currentPoint;
                    prevPointRegion = currentPointRegion;
                    currentPointIndex += isLeftSide ? -1 : 1;
                }

                break;
            }
            else {
                clipped = true;
                prevPoint = currentPoint;
            }
        }

        assert(m_sliceRasterizeEdges[isLeftSide].m_size <= 1);

        //now do the clipping
        for(bool clipped = false; isLeftSide ? currentPointIndex >= 0 : currentPointIndex < unclippedRasterizeEdges.m_size; currentPointIndex += isLeftSide ? -1 : 1) {
            currentPoint = *unclippedRasterizeEdges.m_data[currentPointIndex];

            //if past max vert bounds
            if(gt(currentPoint.y, max.y, m_directionSign[m_dimensionOrder[1]])) {
                //clip segment against the top
                currentPoint.x = lineInterceptX(prevPoint, currentPoint, max.y);
                currentPoint.y = max.y;
                m_debugger.m_clipPoints.push_back(currentPoint);

                clipped = true;
            }

            //find the point's side region
            if(geq(currentPoint.x, min.x, xSign)) {
                if(leq(currentPoint.x, max.x, xSign)) {
                    currentPointRegion = PR_INSIDE;
                }
                else {
                    currentPointRegion = PR_OUTSIDE_MAX;
                }
            }
            else {
                currentPointRegion = PR_OUTSIDE_MIN;
            }

            if(currentPointRegion == PR_OUTSIDE_MIN) { //current point outside min
                if(prevPointRegion == PR_OUTSIDE_MAX) { //previous point outside max
                    //clip segment against max side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, lineInterceptY(prevPoint, currentPoint, max.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    //clip segment against min side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(min.x, lineInterceptY(prevPoint, currentPoint, min.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else if(prevPointRegion == PR_INSIDE) { //previous point inside
                    //clip segment against min side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(min.x, lineInterceptY(prevPoint, currentPoint, min.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else { //previous point outside min
                    //do nothing
                    assert(prevPointRegion == PR_OUTSIDE_MIN);
                }
            }
            else if(currentPointRegion == PR_OUTSIDE_MAX) { //current point outside max
                if(prevPointRegion == PR_OUTSIDE_MIN) { //previous point outside min
                    //clip segment against min side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(min.x, lineInterceptY(prevPoint, currentPoint, min.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    //clip segment against max side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, lineInterceptY(prevPoint, currentPoint, max.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else if(prevPointRegion == PR_INSIDE) { //previous point inside
                    //clip segment against max side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, lineInterceptY(prevPoint, currentPoint, max.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else { //previous point outside max
                    assert(prevPointRegion == PR_OUTSIDE_MAX);
                }

                //if reached max vert bounds, add the corner point
                if(clipped) {
                    m_sliceRasterizeEdges[isLeftSide].add(max);
                    return;
                }
                else if(isLeftSide ? currentPointIndex == 0 : currentPointIndex + 1 == unclippedRasterizeEdges.m_size) {  //if no more points, add the point projected to the side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, currentPoint.y));
                    return;
                }
            }
            else { //current point inside
                assert(currentPointRegion == PR_INSIDE);

                if(prevPointRegion == PR_OUTSIDE_MIN) {
                    //clip segment against min side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(min.x, lineInterceptY(prevPoint, currentPoint, min.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else if(prevPointRegion == PR_OUTSIDE_MAX) {
                    //clip segment against max side
                    m_sliceRasterizeEdges[isLeftSide].add(glm::detail::tvec2<W>(max.x, lineInterceptY(prevPoint, currentPoint, max.x)));
                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);

                    m_debugger.m_clipPoints.push_back(m_sliceRasterizeEdges[isLeftSide].m_data[m_sliceRasterizeEdges[isLeftSide].m_size]);
                }
                else {
                    assert(prevPointRegion = PR_INSIDE);
                }

                m_sliceRasterizeEdges[isLeftSide].add(currentPoint);
            }

            //if reached max vert bounds, done
            if(clipped) {
                return;
            }

            prevPoint = currentPoint;
            prevPointRegion = currentPointRegion;
        }
    }

    bool advanceSlice() {
        if(atEnd()) {
            return true;
        }

        m_currentPosition[m_dimensionOrder[1]] += m_directionSign[1];      

        //advance slice planes
        m_slicePlane.m_distance -= m_cellDimensions[m_dimensionOrder[0]];    //TODO: pretty sure this is going to fail if plane is in a negative quadrant
        m_sliceStart += m_directionSign[m_dimensionOrder[0]] * m_cellDimensions[m_dimensionOrder[0]];

        //swap current point buffers
        m_currentPointList = !m_currentPointList;

        //count down all active edge counts
        //a copy is needed because addPoint can't be modifying the same list that's being updated, horrible bugs happen
        int activeEdgesCopy[FRUSTUM_NUM_EDGES];

        memset(activeEdgesCopy, 0, sizeof(int) * FRUSTUM_NUM_EDGES);

        for(uint8_t activeEdge = 0; activeEdge < FRUSTUM_NUM_EDGES; activeEdge++) {
            if(m_activeEdges[activeEdge] > 0) {
                if(--m_activeEdges[activeEdge] == 0) {    //discard this edge, and add its destination point
                    m_debugger.m_discarededEdges[activeEdge] = true;

                    addPoint(FRUSTUM_EDGE_LIST[activeEdge][m_activeEdgeDestPoint[activeEdge]], activeEdgesCopy);
                }
            }
        }

        for(uint8_t activeEdge = 0; activeEdge < FRUSTUM_NUM_EDGES; activeEdge++) {
            if(activeEdgesCopy[activeEdge] > 0) {
                m_activeEdges[activeEdge] = activeEdgesCopy[activeEdge];
            }
        }

        return setupSlice();
    }

    /**
    Sets up the current slice to be 2D rasterized
    */
    bool setupSlice() {
        //clear other side point buffer
        m_pointList[!m_currentPointList].clear();
        m_debugger.m_pointListMissingDim[!m_currentPointList].clear();

        //find intersection of active edges against other side of slice and add it to the other points list
        for(uint8_t activeEdge = 0; activeEdge < FRUSTUM_NUM_EDGES; activeEdge++) {
            if(m_activeEdges[activeEdge] > 0) {
                assert(m_pointList[!m_currentPointList].m_size < FRUSTUM_NUM_EDGES);

                glm::detail::tvec3<W> dest;

                bool intersection = m_slicePlane.lineIntersection(m_frustum->m_points[FRUSTUM_EDGE_LIST[activeEdge][0]], m_frustum->m_points[FRUSTUM_EDGE_LIST[activeEdge][1]], dest);
                assert(intersection);   //this assert definitely helps find some nasty bugs

                m_pointList[!m_currentPointList].add(glm::detail::tvec2<W>(dest[m_dimensionOrder[2]], dest[m_dimensionOrder[1]]));
                m_debugger.m_pointListMissingDim[!m_currentPointList].add(dest[m_dimensionOrder[0]]);
            }
        }

        /////
        //find convex hull of 2D projected slice points using modified monotone chain algorithm
        //this algorithm is nice because I also have the 2 sides of the polygon I'm about to rasterize in 2D
        //

        //sort points in order of second highest magnitude dimension
        std::list<glm::detail::tvec2<W>*> sortedPoints;

        for(uint8_t list = 0; list < 2; list++) {
            for(uint8_t point = 0; point < m_pointList[list].m_size; point++) {
                sortedPoints.push_back(&m_pointList[list].m_data[point]);
            }
        }

        sortedPoints.sort(PointComparator(*this));
        m_debugger.m_sortedSlicePoints = sortedPoints;

        //find the convex hull and split it into lists of edges for 1 side and the other
        StaticList<glm::detail::tvec2<W>*, FRUSTUM_NUM_EDGES> unclippedRasterizeEdges[2];

        uint8_t secondaryDimension = m_dimensionOrder[1];
        uint8_t tertiaryDimension = m_dimensionOrder[2];

        {
            int8_t convexSign = m_directionSign[secondaryDimension] * m_directionSign[tertiaryDimension];

            //"right" edge
            convexHull(sortedPoints.begin(), sortedPoints.end(), unclippedRasterizeEdges[0], convexSign);

            //"left" edge
            convexHull(sortedPoints.rbegin(), sortedPoints.rend(), unclippedRasterizeEdges[1], convexSign);
        }

        m_debugger.m_unclippedRasterizeEdges[0] = unclippedRasterizeEdges[0];
        m_debugger.m_unclippedRasterizeEdges[1] = unclippedRasterizeEdges[1];

        /////
        //clip convex hull using modified sutherland-hodgman clipping

        m_sliceRasterizeEdges[0].clear();
        m_sliceRasterizeEdges[1].clear();

        m_debugger.m_clipPoints.clear();

        //right edge
        clipHull<RIGHT_SIDE>(unclippedRasterizeEdges[0], glm::detail::tvec2<W>(m_spaceRange.m_min[tertiaryDimension], m_spaceRange.m_min[secondaryDimension]), glm::detail::tvec2<W>(m_spaceRange.m_max[tertiaryDimension], m_spaceRange.m_max[secondaryDimension]), m_directionSign[tertiaryDimension]);

        if(m_sliceRasterizeEdges[0].m_size == 0) {
            //slice is outside the volume
            assert(m_sliceRasterizeEdges[0].m_size == 0);
            assert(m_sliceRasterizeEdges[1].m_size == 0);

            return false;
        }

        assert(m_sliceRasterizeEdges[0].m_size >= 2);

        //left
        clipHull<LEFT_SIDE>(unclippedRasterizeEdges[1], glm::detail::tvec2<W>(m_spaceRange.m_max[tertiaryDimension], m_spaceRange.m_min[secondaryDimension]), glm::detail::tvec2<W>(m_spaceRange.m_min[tertiaryDimension], m_spaceRange.m_max[secondaryDimension]), -m_directionSign[tertiaryDimension]);

        if(m_sliceRasterizeEdges[1].m_size == 0) {
            //slice is outside the volume
            return false;
        }

        //make sure we have a proper polygon, this should only compile into the debug build
        assert(m_sliceRasterizeEdges[0].m_size >= 2);
        assert(m_sliceRasterizeEdges[1].m_size >= 2);

        //clip the minimum vertical side
        if(lt(m_sliceRasterizeEdges[0].m_data[0].y, 
            m_sliceRasterizeEdges[1].m_data[0].y, 
            m_directionSign[secondaryDimension])) {
                m_sliceRasterizeEdges[0].m_data[0].y = m_sliceRasterizeEdges[1].m_data[0].y;
        }
        else if(lt(m_sliceRasterizeEdges[1].m_data[0].y, 
            m_sliceRasterizeEdges[0].m_data[0].y, 
            m_directionSign[secondaryDimension])) {
                m_sliceRasterizeEdges[1].m_data[0].y = m_sliceRasterizeEdges[0].m_data[0].y;
        }

        //clip the maximum vertical side
        if(gt(m_sliceRasterizeEdges[0].m_data[m_sliceRasterizeEdges[0].m_size - 1].y, 
            m_sliceRasterizeEdges[1].m_data[m_sliceRasterizeEdges[1].m_size - 1].y, 
            m_directionSign[secondaryDimension])) {
                m_sliceRasterizeEdges[0].m_data[m_sliceRasterizeEdges[0].m_size - 1].y = m_sliceRasterizeEdges[1].m_data[m_sliceRasterizeEdges[1].m_size - 1].y;
        }
        else if(gt(m_sliceRasterizeEdges[1].m_data[m_sliceRasterizeEdges[1].m_size - 1].y, 
            m_sliceRasterizeEdges[0].m_data[m_sliceRasterizeEdges[0].m_size - 1].y, 
            m_directionSign[secondaryDimension])) {
                m_sliceRasterizeEdges[1].m_data[m_sliceRasterizeEdges[1].m_size - 1].y = m_sliceRasterizeEdges[0].m_data[m_sliceRasterizeEdges[0].m_size - 1].y;
        }

        //setup the initial row

        m_activeSliceEdgeIndex[RIGHT_SIDE] = 0;
        m_activeSliceEdgeIndex[LEFT_SIDE] = 0;

        //sets up the min vertical
        int rowNum = gridDistance(m_sliceRasterizeEdges[0].m_data[0].y - m_spaceRange.m_min[secondaryDimension], secondaryDimension);      
        m_currentPosition[secondaryDimension] = m_range.m_min[secondaryDimension] + rowNum * m_directionSign[secondaryDimension];

        m_debugger.m_sliceMin.y = m_currentPosition[secondaryDimension];

        m_lineBottom = m_spaceRange.m_min[secondaryDimension] + rowNum * m_directionSign[secondaryDimension] * m_cellDimensions[secondaryDimension];
        m_lineTop = m_lineBottom + m_directionSign[secondaryDimension] * m_cellDimensions[secondaryDimension];

        //sets up the max vertical
        rowNum = gridDistance(m_sliceRasterizeEdges[0].m_data[m_sliceRasterizeEdges[0].m_size - 1].y - m_spaceRange.m_min[secondaryDimension], secondaryDimension);
        m_sliceMax.y = m_range.m_min[secondaryDimension] + rowNum * m_directionSign[secondaryDimension];

        //sets up some initial horizontal
        m_sliceMax.x = m_range.m_min[tertiaryDimension];
        m_currentPosition[tertiaryDimension] = m_range.m_max[tertiaryDimension];

        addSlicePoint<RIGHT_SIDE>();
        addSlicePoint<LEFT_SIDE>();

        setupRow();

        return true;
    }

    void advanceRow() {
        //advance row locations
        m_lineBottom = m_lineTop;
        m_lineTop += m_directionSign[m_dimensionOrder[1]] * m_cellDimensions[m_dimensionOrder[1]];

        m_currentPosition[m_dimensionOrder[1]] += m_directionSign[m_dimensionOrder[1]];

        m_sliceMax.x = m_range.m_min[m_dimensionOrder[2]];
        m_currentPosition[m_dimensionOrder[2]] = m_range.m_max[m_dimensionOrder[2]];

        //count down the active edges
        if(--m_activeSliceEdges[RIGHT_SIDE] == 0 && m_activeSliceEdgeIndex[RIGHT_SIDE] + 1 < m_sliceRasterizeEdges[RIGHT_SIDE].m_size - 1) {
            m_activeSliceEdgeIndex[RIGHT_SIDE] ++;
            addSlicePoint<RIGHT_SIDE>();
        }

        if(--m_activeSliceEdges[LEFT_SIDE] == 0 && m_activeSliceEdgeIndex[LEFT_SIDE] + 1 < m_sliceRasterizeEdges[LEFT_SIDE].m_size - 1) {
            m_activeSliceEdgeIndex[LEFT_SIDE] ++;
            addSlicePoint<LEFT_SIDE>();
        }

        setupRow();
    }

    /**
    Sets up the current slice row to be 2D rasterized
    */
    void setupRow() {
        W xIntercept;

        //make sure the edge indexes are pointing to no more than the second to last point in each edge list 
        assert(m_activeSliceEdgeIndex[RIGHT_SIDE] + 1 < m_sliceRasterizeEdges[RIGHT_SIDE].m_size);
        assert(m_activeSliceEdgeIndex[LEFT_SIDE] + 1 < m_sliceRasterizeEdges[LEFT_SIDE].m_size);

        //set the right side
        xIntercept = lineInterceptX(m_sliceRasterizeEdges[RIGHT_SIDE].m_data[m_activeSliceEdgeIndex[RIGHT_SIDE]], m_sliceRasterizeEdges[RIGHT_SIDE].m_data[m_activeSliceEdgeIndex[RIGHT_SIDE] + 1], m_acticeSliceEdgeOutward[RIGHT_SIDE] ? m_lineTop : m_lineBottom);

        P column = m_range.m_min[m_dimensionOrder[2]] + gridDistance(xIntercept - m_spaceRange.m_min[m_dimensionOrder[2]], m_dimensionOrder[2]) * m_directionSign[m_dimensionOrder[2]];

        if(gt(column, m_sliceMax.x, m_directionSign[2])) {
            m_sliceMax.x = column;
        }

        //set the left side
        xIntercept = lineInterceptX(m_sliceRasterizeEdges[LEFT_SIDE].m_data[m_activeSliceEdgeIndex[LEFT_SIDE]], m_sliceRasterizeEdges[LEFT_SIDE].m_data[m_activeSliceEdgeIndex[LEFT_SIDE] + 1], m_acticeSliceEdgeOutward[LEFT_SIDE] ? m_lineTop : m_lineBottom);

        column = m_range.m_min[m_dimensionOrder[2]] + gridDistance(xIntercept - m_spaceRange.m_min[m_dimensionOrder[2]], m_dimensionOrder[2]) * m_directionSign[m_dimensionOrder[2]];

        if(lt(column, m_currentPosition[m_dimensionOrder[2]], m_directionSign[2])) {
            m_currentPosition[m_dimensionOrder[2]] = column;
        }

        m_debugger.m_sliceMin.x = column;

        m_debugger.m_rasterizedCells.push_back(m_cellDimensions * vec3cast<P, W>(m_currentPosition) + m_cellDimensions * 0.5f * vec3cast<int8_t, W>(m_directionSign));
    }

    ///Range of 3D cells being rasterized
    Box<P> m_range;

    ///The current grid cell the iterator is pointing to
    glm::detail::tvec3<P> m_currentPosition;

    ///The range in physical space, derived from m_range * m_cellDimensions
    Box<W> m_spaceRange;

    ///Dimensions of the cells 
    glm::detail::tvec3<W> m_cellDimensions;

    ///Sorted dimension indeces by order of magnitude of the direction vector the view frustum faces
    glm::detail::tvec3<uint8_t> m_dimensionOrder;
    ///Sign of each dimension in the direction vector the view frustum faces
    glm::detail::tvec3<int8_t> m_directionSign;

    ///Edges that have yet to be processed by the rasterizing algorithm
    bool m_inactiveEdges[FRUSTUM_NUM_EDGES];

    ///Edges that are currently being processed by the rasterizing algorithm
    int m_activeEdges[FRUSTUM_NUM_EDGES];

    ///The active edge point index that the slice is going towards (It's boolean because it's either point 0 or point 1 in the edge list)
    bool m_activeEdgeDestPoint[FRUSTUM_NUM_EDGES];

    ///The frustum itself
    const Frustum<W>* m_frustum;

    ///The points that make up the slice of the frustum currently being rasterized
    StaticList<glm::detail::tvec2<W>, FRUSTUM_NUM_EDGES> m_pointList[2];
    ///Index of which of the points lists is the one from the previous slice front side
    bool m_currentPointList;

    ///The current backside of the frustum slice
    W m_sliceStart;
    ///The plane representing the sliceEnd so you can clip edges against it 
    Plane<W> m_slicePlane;

    ///The current "bottom" of the rasterizing polygon
    W m_lineBottom;

    ///The current "top" of the rasterizing polygon
    W m_lineTop;

    ///The current maximums for the 2D rasterizing, the x value changes at every row, the slice is done being rasterized when m_currentPosition[m_dimensionOrder[1]] y of sliceMax is reached
    glm::detail::tvec2<P> m_sliceMax;

    ///The edge lists for the convex polygon rasterizing
    StaticList<glm::detail::tvec2<W>, FRUSTUM_NUM_EDGES> m_sliceRasterizeEdges[2];

    ///The index of the first point of the active edge
    uint8_t m_activeSliceEdgeIndex[2];

    ///The countdown of how many rows until the next point edge is reached
    int m_activeSliceEdges[2];

    ///Whether or not the current active edge for each side is heading away from or towards the center of the polygon's middle vertical axis
    bool m_acticeSliceEdgeOutward[2];

    ///For debugging, remove later
    Debugger m_debugger;
};

#endif