/* Same as flipttransformCyl_NPparab but implemented as a MEX file
 * Also, modified for use with the TI LightCrafter DLP 3010 EVM projector.
 */

#include <math.h>
#include <stdlib.h>

#include "mex.h"
#include "matrix.h"

#define THROW_RATIO 1.2
#define W_OVER_H (16.0/9)
#define PARAB_A (-0.125)
#define PARAB_C 5
#define SCREEN_WIDTH 15.0
#define SCREEN_HEIGHT 8.0
#define PI (acos(-1))
#define PROJ_TILT (12.042 * PI / 180)  // downward angle of projector

// assume that at the center (closest part of screen), image fills screen vertically
#define ORTHO_HEIGHT_TO_BASE_DIST (W_OVER_H * THROW_RATIO)
#define PROJ_FULL_ANGLE (atan2(1.0, ORTHO_HEIGHT_TO_BASE_DIST))
#define PLANE_HEIGHT_TO_ORTHO_HEIGHT (cos(PROJ_TILT))
#define PLANE_HEIGHT_TO_BASE_DIFF (sin(PROJ_TILT))

#define MIN_ORTHO_HEIGHT (PLANE_HEIGHT_TO_ORTHO_HEIGHT * SCREEN_HEIGHT)
#define MIN_BASE_DIST (ORTHO_HEIGHT_TO_BASE_DIST * MIN_ORTHO_HEIGHT)
#define MIN_BASE_DIFF (PLANE_HEIGHT_TO_BASE_DIFF * SCREEN_HEIGHT)
#define MIN_PROJ_DIST (cos(PROJ_TILT) * (MIN_BASE_DIST + MIN_BASE_DIFF))
#define PROJ_DIST_TO_PLANE_HEIGHT (SCREEN_HEIGHT / MIN_PROJ_DIST)
#define PROJ_DIST_TO_BASE_DIST (PROJ_DIST_TO_PLANE_HEIGHT * PLANE_HEIGHT_TO_ORTHO_HEIGHT * ORTHO_HEIGHT_TO_BASE_DIST)
#define PROJ_DIST_TO_BASE_DIFF (PROJ_DIST_TO_PLANE_HEIGHT * PLANE_HEIGHT_TO_BASE_DIFF)

#define DIST_TO_MOUSE (MIN_PROJ_DIST + PARAB_C)


void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    mxAssert(nrhs == 2, "Transformation function takes 2 arguments");

    // inmexPrintf
    mwSize ncols = mxGetN(prhs[0]);  // Number of points to transform
    mxDouble *coord3 = mxGetDoubles(prhs[0]);

    // parameters in vr struct
    mxArray* exper = mxGetField(prhs[1], 0, "exper");
    mxArray* vars = mxGetProperty(exper, 0, "variables");
    
    // Offset angle (degrees projector is turned to the left)
    mxArray* projYaw_arr = mxGetField(vars, 0, "projectorYaw");
    double projYaw = 0;
    if (projYaw_arr) {  // NULL if field does not exist
        char *projYawStr = mxArrayToString(projYaw_arr);
        if (projYawStr) {
            projYaw = atof(projYawStr) * PI / 180;
            mxFree(projYawStr);
        }
    }

    // outmexPrintf
    plhs[0] = mxCreateDoubleMatrix(3, ncols, mxREAL);
    mxDouble *coord3new = mxGetDoubles(plhs[0]);


    for (int i = 0; i < ncols; i++) {
        double objX = coord3[3*i];
        double objY = coord3[3*i+1];
        double objZ = coord3[3*i+2];

        double *screenX = coord3new + 3*i;
        double *screenY = screenX + 1;
        double *isVisible = screenX + 2;
        *isVisible = 1;

        // distance to object in X/Y plane
        double hypotxy = hypot(objX, objY);
        double azimuth = atan2(objY, objX);
        if (hypotxy == 0 || (azimuth < -PI/4 && azimuth > (-PI + PI/4))) {
            *isVisible = 0;
        }

        // Put azimuth in the 1st quadrant, keeping track of signs
        double newX;
        if (objX == 0) {
            // Special case - transformed X is still 0
            newX = 0;
        } else {
            double absY = fabs(objY);
            double absX = fabs(objX);
            double ySign = 0, xSign = objX / absX;
            if (objY != 0) { ySign = objY / absY; }
            double tanAzimuth = absY / absX;

            // Solve quadratic equation - y should equal x * tan(azimuth),
            // corrected for sign, to be on the screen.
            double a = PARAB_A, c = PARAB_C, b = -tanAzimuth * ySign;
            newX = ((-b - sqrt(b*b - 4*a*c)) / (2*a)) * xSign;
        }

        double newY = PARAB_A*newX*newX + PARAB_C;
        double projDist = DIST_TO_MOUSE - newY;
        double screenDist = hypot(newX, newY);

        // Now translate the location on the screen into image coordinates for the projector
        // projection of z to image location on screen
        double planeHeight = PROJ_DIST_TO_PLANE_HEIGHT * projDist;
        double heightOnScreen = -objZ / hypotxy * screenDist;  // invert here
        double inchFromTop = planeHeight/2 - heightOnScreen;
        double fracFromTop = inchFromTop / planeHeight;
        double baseDist = PROJ_DIST_TO_BASE_DIST * projDist;
        double baseDiff = PROJ_DIST_TO_BASE_DIFF * projDist;
        baseDist +=  baseDiff * fracFromTop;
        
        // angle correction (oops)
        double orthoHeight = PROJ_DIST_TO_PLANE_HEIGHT * PLANE_HEIGHT_TO_ORTHO_HEIGHT * projDist;
        double centerDistToVertical = sqrt(baseDist*baseDist + orthoHeight*orthoHeight);
        double correctAngle = atan2(newX, centerDistToVertical);
        double currentAngle = correctAngle + projYaw;
        double currentOffset = centerDistToVertical * (tan(currentAngle) -
                newX / centerDistToVertical);
        newX += currentOffset;  // add instead of subtract b/c projector reverses x

        double projectedImageWidth = baseDist / THROW_RATIO;
        double projectedImageHeight = projectedImageWidth / W_OVER_H;

        // screenX and screenY are both expressed as multiples of half the imageHeight
        *screenX = newX * 2 / projectedImageHeight;
        if (fabs(newX) > 1.2 * SCREEN_WIDTH / 2) {
            *isVisible = 0;
        }

        // find height on projected (ortho) plane
        double inchFromBottomOrtho = planeHeight * PLANE_HEIGHT_TO_ORTHO_HEIGHT * (1 - fracFromTop);
        double inchFromCenterOrtho = inchFromBottomOrtho - projectedImageHeight / 2;

        // negate heightOnScreen to flip
        *screenY = inchFromCenterOrtho * 2 / projectedImageHeight;
        if (fabs(heightOnScreen) > SCREEN_HEIGHT / 2) {
            *isVisible = 0;
        }
    }
}
