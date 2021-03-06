/**
 * @file 
 * @brief Representation of an octree for show
 * @author Jan Elseberg. Jacobs University Bremen gGmbH, Germany
 * @author Andreas Nuechter. Jacobs University Bremen gGmbH, Germany
 */

#ifndef SHOWBOCTREE_H
#define SHOWBOCTREE_H

#include "slam6d/Boctree.h"
#include "show/colormanager.h"
#include "show/scancolormanager.h"
#include "show/viewcull.h"
#include "show/colordisplay.h"


class ScanColorManager;

/**
 * @brief Octree for show
 * 
 * A cubic bounding box is calculated
 * from the given 3D points. Then it
 * is recusivly subdivided into smaller
 * subboxes
 *
 * It contains software culling functionalities
 */
template <class T> class Show_BOctTree : public BOctTree<T>, public colordisplay  {

public:

  template <class P>
  Show_BOctTree(P * const* pts, int n, T voxelSize, PointType _pointtype = PointType(), ScanColorManager *scm = 0)
    : BOctTree<T>(pts, n, voxelSize, _pointtype, true) {
    setColorManager(0);
    if (scm) {
      scm->registerTree(this);
      for (int i = 1; i < n; i++) {
        scm->updateRanges(pts[i]);
      }
    }
    maxtargetpoints =  maxTargetPoints(*BOctTree<T>::root);
    current_lod_mode = 0;
  }

  Show_BOctTree(std::string filename, ScanColorManager *scm = 0) : BOctTree<T>(filename)
  {
    if (scm) {
      scm->registerTree(this);
      scm->updateRanges(BOctTree<T>::mins);
      scm->updateRanges(BOctTree<T>::maxs);
    }
    setColorManager(0);
    maxtargetpoints =  maxTargetPoints(*BOctTree<T>::root);
    current_lod_mode = 0;
  }

  void displayOctTreeCulled(long targetpts) { 
    glBegin(GL_POINTS);
    displayOctTreeCulledLOD(targetpts, *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size); 
    glEnd();
  }

  void drawLOD(float ratio) { 
    switch (current_lod_mode) {
      case 0:
        glBegin(GL_POINTS);
        displayOctTreeCulledLOD(maxtargetpoints * ratio, *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size); 
        glEnd();
        break;
      case 1:
        glBegin(GL_POINTS);
        displayOctTreeCulledLOD2(ratio , *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size);
        glEnd();
        break;
      case 2:
#ifdef WITH_GLEE
        if (GLEE_ARB_point_parameters) {
          glPointParameterfARB(GL_POINT_SIZE_MIN_ARB, 1.0);
          glPointParameterfARB(GL_POINT_SIZE_MAX_ARB, 100000.0);
          GLfloat p[3] = {0.0, 0.0000, 0.0000005};
          glPointParameterfvARB(GL_POINT_DISTANCE_ATTENUATION_ARB, p);
          displayOctTreeCPAllCulled(*BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size,  BOctTree<T>::size/  pow(2, min( (int)(ratio * BOctTree<T>::max_depth ), BOctTree<T>::max_depth - 3) ) ); 
          p[0] = 1.0;
          p[2] = 0.0;
          glPointParameterfvARB(GL_POINT_DISTANCE_ATTENUATION_ARB, p);
        }
#endif
      break;
      default:
      break;
    }
  }
  
  void draw() { 
    glBegin(GL_POINTS);
    displayOctTreeAllCulled(*BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size); 
    glEnd();
  }
  
  void displayOctTree(T minsize = FLT_MAX) {
    displayOctTreeCAllCulled(*BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size, minsize); 
  }

  void selectRayBrushSize(set<T *> &points, int brushsize) { 
    selectRayBS(points, *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size, brushsize); 
  }
  void selectRay(set<T *> &points, int depth = INT_MAX) { 
    selectRay(points, *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size, depth); 
  }
  void selectRay(T * &point) { 
    selectRay(point, *BOctTree<T>::root, BOctTree<T>::center, BOctTree<T>::size, FLT_MAX); 
  }


  void cycleLOD() {
    current_lod_mode = (current_lod_mode+1)%3;
  }


protected:
  unsigned long maxTargetPoints( bitoct &node ) {
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    unsigned long max = 0;

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned long length = points[0].length;
          if (length > max) max = length;
        } else { // recurse
          unsigned long tp = maxTargetPoints( children->node);
          if (tp > max) max = tp;
        }
        ++children; // next child
      }
    }

    return max*POPCOUNT(node.valid);
  }
  
  void displayOctTreeAll( bitoct &node) {
//    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point
          for(unsigned int iterator = 0; iterator < length; iterator++ ) {
            if(cm) cm->setColor(point);
            glVertex3f( point[0], point[1], point[2]);
            point+=BOctTree<T>::POINTDIM;
          }
        } else { // recurse
          displayOctTreeAll( children->node);
        }
        ++children; // next child
      }
    }
  }

  void displayOctTreeAllCulled( bitoct &node, T *center, T size ) {
    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeAll(node);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
       //   if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
              point+=BOctTree<T>::POINTDIM;
            }
          //}
        } else { // recurse
          displayOctTreeAllCulled( children->node, ccenter, size/2.0);
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeLOD2(float ratio,  bitoct &node, T *center, T size ) {

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point

          int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
          l = max((int)(l*l*ratio), 0);
          if (l > 1) {
            if ((int)length > l ) {
              T each = (T)BOctTree<T>::POINTDIM * (T)((T)length/(T)l);
              T *p;
              int index;
              for(int iterator = 0; iterator < l; iterator++ ) {
                index = (T)iterator * each;
                p = point + index - index%BOctTree<T>::POINTDIM;
                if(cm) cm->setColor(p);
                glVertex3f( p[0], p[1], p[2]);
              }
            } else if ((int)length <= l) { 
              for(unsigned int iterator = 0; iterator < length; iterator++ ) {
                if(cm) cm->setColor(point);
                glVertex3f( point[0], point[1], point[2]);
                point+=BOctTree<T>::POINTDIM;
              }
            } /* else if (l == 1) {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
            }*/
          } else {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
          }
        } else { // recurse
            int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
            l = max((int)(l*l*ratio), 0);
            if (l > 0) {
              displayOctTreeCulledLOD2(ratio, children->node, ccenter, size/2.0);
            }
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeCulledLOD2(float ratio, bitoct &node, T *center, T size ) {

    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeLOD2(ratio, node, center, size);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point

            int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
            l = max((int)(l*l*ratio), 0);
            if (l != 0) {
              if ((int)length > l ) {
                T each = (T)BOctTree<T>::POINTDIM * (T)((T)length/(T)l);
                T *p;
                int index;
                for(int iterator = 0; iterator < l; iterator++ ) {
                  index = (T)iterator * each;
                  p = point + index - index%BOctTree<T>::POINTDIM;
                  if(cm) cm->setColor(p);
                  glVertex3f( p[0], p[1], p[2]);
                }
              } else if ((int)length <= l) { 
                for(unsigned int iterator = 0; iterator < length; iterator++ ) {
                  if(cm) cm->setColor(point);
                  glVertex3f( point[0], point[1], point[2]);
                  point+=BOctTree<T>::POINTDIM;
                }
              } else if (l == 1) {
                if(cm) cm->setColor(point);
                glVertex3f( point[0], point[1], point[2]);
              }
            } 
          }
        } else { // recurse
          //int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
          //l = max((int)(l*l*ratio), 0);
          //if (l > 0) {
            displayOctTreeCulledLOD2(ratio, children->node, ccenter, size/2.0);
          //}
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeLOD3(long targetpts, bitoct &node, T *center, T size ) {
    if (targetpts <= 0) return; // no need to display anything

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point
          int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
          
          if ( l <= targetpts) {  // only a single pixel on screen only paint one point
            if(cm) cm->setColor(point);
            glVertex3f( point[0], point[1], point[2]);
          } else { 
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
              point+=BOctTree<T>::POINTDIM;
            }
          }
        } else { // recurse
          displayOctTreeLOD3(targetpts, children->node, ccenter, size/2.0);
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeCulledLOD3(long targetpts, bitoct &node, T *center, T size ) {
    if (targetpts <= 0) return; // no need to display anything

    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeLOD3(targetpts, node, center, size);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    
    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        //l = std::min( max((int)(l*l*ratio), 1), targetpts);
        
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point

            int l = LOD2(ccenter[0], ccenter[1], ccenter[2], size/2.0);  // only a single pixel on screen only paint one point
          if (targetpts <= 0 ) cout << l << " " << targetpts << endl;
            if ( l <= targetpts) {  // only a single pixel on screen only paint one point
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
            } else { 
              for(unsigned int iterator = 0; iterator < length; iterator++ ) {
                if(cm) cm->setColor(point);
                glVertex3f( point[0], point[1], point[2]);
                point+=BOctTree<T>::POINTDIM;
              }
            }
          }

        } else { // recurse
          displayOctTreeCulledLOD3(targetpts, children->node, ccenter, size/2.0);
        }
        ++children; // next child
      }
    }
  }

  void displayOctTreeCulledLOD(long targetpts, bitoct &node, T *center, T size ) {
    if (targetpts <= 0) return; // no need to display anything

    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeLOD(targetpts, node, center, size);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    unsigned short nc = POPCOUNT(node.valid);
    long newtargetpts = targetpts;
    if (nc > 0) {
      newtargetpts = newtargetpts/nc;
      if (newtargetpts <= 0 ) return;
    }

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point

            if (length > 10 && !LOD(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {  // only a single pixel on screen only paint one point
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
            } else if (length <= newtargetpts) {        // more points requested than possible, plot all
              for(unsigned int iterator = 0; iterator < length; iterator++ ) {
                if(cm) cm->setColor(point);
                glVertex3f( point[0], point[1], point[2]);
                point+=BOctTree<T>::POINTDIM;
              }
            } else {                         // select points to show
              // TODO smarter subselection of points here
              T each = (T)BOctTree<T>::POINTDIM * (T)((T)length/(T)newtargetpts);
              T *p;
              int index;
              for(unsigned int iterator = 0; iterator < newtargetpts; iterator++ ) {
                index = (T)iterator * each;
                p = point + index - index%BOctTree<T>::POINTDIM;
                if(cm) cm->setColor(p);
                glVertex3f( p[0], p[1], p[2]);
                //point += each;
              }
            }
          }

        } else { // recurse
          displayOctTreeCulledLOD(newtargetpts, children->node, ccenter, size/2.0);
        }
        ++children; // next child
      }
    }
  }

  void displayOctTreeLOD(long targetpts, bitoct &node, T *center, T size ) {
    if (targetpts <= 0) return; // no need to display anything

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    unsigned short nc = POPCOUNT(node.valid);
    long newtargetpts = targetpts;
    if (nc > 0) {
      newtargetpts = newtargetpts/nc;
      if (newtargetpts <= 0 ) return;
    }


    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point
          if (length > 10 && !LOD(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {  // only a single pixel on screen only paint one point
            if(cm) cm->setColor(point);
            glVertex3f( point[0], point[1], point[2]);
          } else if (length <= newtargetpts) {        // more points requested than possible, plot all
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
              point+=BOctTree<T>::POINTDIM;
            }
          } else {                         // select points to show
            // TODO smarter subselection of points here
            T each = (T)BOctTree<T>::POINTDIM * (T)((T)length/(T)newtargetpts);
            T *p;
            int index;
            for(unsigned int iterator = 0; iterator < newtargetpts; iterator++ ) {
              index = (T)iterator * each;
              p = point + index - index%BOctTree<T>::POINTDIM;
              if(cm) cm->setColor(p);
              glVertex3f( p[0], p[1], p[2]);
              //point += each;
            }
          }
        } else { // recurse
          displayOctTreeLOD(newtargetpts, children->node, ccenter, size/2.0);
        }
        ++children; // next child
      }
    }
  }
  void selectRay(set<T *> &selpoints, bitoct &node, T *center, T size, int max_depth, int depth = 0) {
    if (depth < max_depth &&  !HitBoundingBox(center, size ))return;

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( !(depth+1  < max_depth) || HitBoundingBox(ccenter, size) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
                selpoints.insert(point);
              point+=BOctTree<T>::POINTDIM;
            }
          }
        } else { // recurse
          selectRay( selpoints, children->node, ccenter, size/2.0, max_depth, depth+1);
        }
        ++children; // next child
      }
    }
  }

  void selectRayBS(set<T *> &selpoints, bitoct &node, T *center, T size, int brushsize) {
    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( HitBoundingBox(ccenter, size) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              if (ScreenDist(point) < brushsize && RayDist(point) > 100.0) 
                selpoints.insert(point);
              point+=BOctTree<T>::POINTDIM;
            }
          }
        } else { // recurse
          selectRayBS( selpoints, children->node, ccenter, size/2.0, brushsize);
        }
        ++children; // next child
      }
    }
  }

  void selectRay(T * &selpoint, bitoct &node, T *center, T size, float min) {
    if (!HitBoundingBox(center, size ))return;

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( HitBoundingBox(ccenter, size) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              T dist = RayDist(point);
              if (min > dist && ScreenDist(point) < 5 && dist > 100.0) {
                selpoint = point;
                min = dist;
              }
              point+=BOctTree<T>::POINTDIM;
            }
          }
        } else { // recurse
          selectRay( selpoint, children->node, ccenter, size/2.0, min);
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeCAllCulled( bitoct &node, T *center, T size, T minsize ) {
    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeCAll(node, center, size, minsize);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf || minsize > size ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            showCube(ccenter, size/2.0);
          }
        } else { // recurse
          displayOctTreeCAllCulled( children->node, ccenter, size/2.0, minsize);
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeCAll( bitoct &node, T *center, T size, T minsize ) {
    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf || minsize > size ) {   // if ith node is leaf get center
          showCube(ccenter, size/2.0);
        } else { // recurse
          displayOctTreeCAll( children->node, ccenter, size/2.0, minsize);
        }
        ++children; // next child
      }
    }
  }
  
  
  void displayOctTreeCPAllCulled( bitoct &node, T *center, T size, T minsize ) {
    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return;  // culled do not continue with this branch of the tree

    if (res == 2) { // if entirely within frustrum discontinue culling
      displayOctTreeCPAll(node, center, size, minsize);
      return;
    }

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if ( minsize > size ) {
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            if(cm) {
              if (( 1 << i ) & node.leaf ) 
                cm->setColor( &(children->points[1].v) );
              else 
                cm->setColor( pickPoint(children->node) );
            }

            glPointSize(size/2.0);
            glBegin(GL_POINTS);
            glVertex3f( ccenter[0], ccenter[1], ccenter[2] ); 
            glEnd();
          }
        }else if ( ( 1 << i ) & node.leaf ) {
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            T *point = &(points[1].v);  // first point
            glPointSize(1.0);
            glBegin(GL_POINTS);
            for(unsigned int iterator = 0; iterator < length; iterator++ ) {
              if(cm) cm->setColor(point);
              glVertex3f( point[0], point[1], point[2]);
              point+=BOctTree<T>::POINTDIM;
            }
            glEnd();
          }
        } else { // recurse
          displayOctTreeCPAllCulled( children->node, ccenter, size/2.0, minsize);
        }
        ++children; // next child
      }
    }
  }
  
  void displayOctTreeCPAll( bitoct &node, T *center, T size, T minsize ) {
    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if ( minsize > size ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            if(cm) {
              if (( 1 << i ) & node.leaf ) 
                cm->setColor( &(children->points[1].v) );
              else 
                cm->setColor( pickPoint(children->node) );
            }

            glPointSize(size/2.0);
            glBegin(GL_POINTS);
            glVertex3f( ccenter[0], ccenter[1], ccenter[2] ); 
            glEnd();
          }
        }else if ( ( 1 << i ) & node.leaf ) {
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point
          glPointSize(1.0);
          glBegin(GL_POINTS);
          for(unsigned int iterator = 0; iterator < length; iterator++ ) {
            if(cm) cm->setColor(point);
            glVertex3f( point[0], point[1], point[2]);
            point+=BOctTree<T>::POINTDIM;
          }
          glEnd();
        } else { // recurse
          displayOctTreeCPAll( children->node, ccenter, size/2.0, minsize);
        }
        ++children; // next child
      }
    }
  }

  void showCube(T *center, T size) {
    glLineWidth(1.0);
    glBegin(GL_QUADS);      // draw a cube with 6 quads
    glColor3f(0.0f,1.0f,0.0f);      // Set The Color To Green
    glVertex3f(center[0] + size, center[1] + size, center[2] - size);
    glVertex3f(center[0] - size, center[1] + size, center[2] - size);
    glVertex3f(center[0] - size, center[1] + size, center[2] + size);
    glVertex3f(center[0] + size, center[1] + size, center[2] + size);
    glColor3f(1.0f,0.5f,0.0f);      // Set The Color To Orange

    glVertex3f(center[0] + size, center[1] - size, center[2] + size); 
    glVertex3f(center[0] - size, center[1] - size, center[2] + size);
    glVertex3f(center[0] - size, center[1] - size, center[2] - size);
    glVertex3f(center[0] + size, center[1] - size, center[2] - size);

    glColor3f(1.0f,0.0f,0.0f);      // Set The Color To Red
    glVertex3f(center[0] + size, center[1] + size, center[2] + size); 
    glVertex3f(center[0] - size, center[1] + size, center[2] + size);
    glVertex3f(center[0] - size, center[1] - size, center[2] + size);
    glVertex3f(center[0] + size, center[1] - size, center[2] + size);

    glColor3f(1.0f,1.0f,0.0f);      // Set The Color To Yellow
    glVertex3f(center[0] + size, center[1] - size, center[2] - size); 
    glVertex3f(center[0] - size, center[1] - size, center[2] - size);
    glVertex3f(center[0] - size, center[1] + size, center[2] - size);
    glVertex3f(center[0] + size, center[1] + size, center[2] - size);

    glColor3f(0.0f,0.0f,1.0f);      // Set The Color To Blue
    glVertex3f(center[0] - size, center[1] + size, center[2] + size); 
    glVertex3f(center[0] - size, center[1] + size, center[2] - size);
    glVertex3f(center[0] - size, center[1] - size, center[2] - size);
    glVertex3f(center[0] - size, center[1] - size, center[2] + size);

    glColor3f(1.0f,0.0f,1.0f);      // Set The Color To Violet
    glVertex3f(center[0] + size, center[1] + size, center[2] - size); 
    glVertex3f(center[0] + size, center[1] + size, center[2] + size);
    glVertex3f(center[0] + size, center[1] - size, center[2] + size);
    glVertex3f(center[0] + size, center[1] - size, center[2] - size);

    glEnd();
  }
  
  
  void displayOctTreeAllCulled( bitoct &node, T *center, T size, float *frustum[6], unsigned char frustsize ) {
    float *new_frustum[6]; unsigned char counter = 0;
    for (unsigned char p = 0; p < frustsize; p++ ) {
      char res = PlaneAABB(center[0], center[1], center[2], size, frustum[p]);
      if (res == 0) { // cube is on the wrong side of the plane (not visible)
        return;
      } else if ( res == 1 ) {  // plane intersects this volume continue culling with this plane
        new_frustum[counter++] = frustum[p];
      } // other case is simply not to continue culling with the respective plane
    }
    if (counter == 0) { // if entirely within frustrum discontinue culling
      displayOctTreeAll(node);
      return;
    }
    
    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          pointrep *points = children->points;
          unsigned int length = points[0].length;
          T *point = &(points[1].v);  // first point
          for(unsigned int iterator = 0; iterator < length; iterator++ ) {
            if(cm) cm->setColor(point);
            glVertex3f( point[0], point[1], point[2]);
            point+=BOctTree<T>::POINTDIM;
          }
        } else { // recurse
          displayOctTreeAllCulled( children->node, ccenter, size/2.0, new_frustum, counter);
        }
        ++children; // next child
      }
    }
  }

  unsigned long int countVisiblePoints( bitoct &node, T *center, T size ) {
    unsigned long int result = 0;
    int res = CubeInFrustum2(center[0], center[1], center[2], size);
    if (res==0) return 0;  // culled do not continue with this branch of the tree

    T ccenter[3];
    bitunion<T> *children;
    bitoct::getChildren(node, children);

    for (short i = 0; i < 8; i++) {
      if (  ( 1 << i ) & node.valid ) {   // if ith node exists
        childcenter(center, ccenter, size, i);  // childrens center
        if (  ( 1 << i ) & node.leaf ) {   // if ith node is leaf get center
          // check if leaf is visible
          if ( CubeInFrustum(ccenter[0], ccenter[1], ccenter[2], size/2.0) ) {
            pointrep *points = children->points;
            unsigned int length = points[0].length;
            result += length;
          }
        } else { // recurse
          result += countVisiblePoints( children->node, ccenter, size/2.0);
        }   
        ++children; // next child
      }
    }         
    return result;
  }           

  unsigned long maxtargetpoints;

  unsigned int current_lod_mode;

};

#endif
