
#ifndef __CD_APPLET_STRUCT__
#define  __CD_APPLET_STRUCT__

#include <cairo-dock.h>

#define CD_ANIMATIONS_SPOT_HEIGHT 12

typedef enum {
	CD_SQUARE_MESH=0,
	CD_CUBE_MESH,
	CD_CAPSULE_MESH,
	CD_ANIMATIONS_NB_MESH
	} CDAnimationsMeshType;

typedef enum {
	CD_HORIZONTAL_STRECTH=0,
	CD_VERTICAL_STRECTH,
	CD_CORNER_STRECTH,
	CD_ANIMATIONS_NB_STRECTH
	} CDAnimationsStretchType;

//\___________ structure containing the applet's configuration parameters.
struct _AppletConfig {
	gint iRotationDuration;
	gboolean bContinueRotation;
	CDAnimationsMeshType iMeshType;
	GLfloat pMeshColor[4];
	
	gint iSpotDuration;
	gboolean bContinueSpot;
	GLfloat pSpotColor[3];
	GLfloat pHaloColor[4];
	gdouble pRaysColor1[3];
	gdouble pRaysColor2[3];
	gboolean bMysticalRays;
	gint iNbRaysParticles;
	gint iRaysParticleSize;
	gdouble fRaysParticleSpeed;
	
	gint iNbGridNodes;
	CDAnimationsStretchType iInitialStrecth;
	gdouble fSpringConstant;
	gdouble fFriction;
	} ;

//\___________ structure containing the applet's data, like surfaces, dialogs, results of calculus, etc.
struct _AppletData {
	GLuint iChromeTexture;
	GLuint iCallList[CD_ANIMATIONS_NB_MESH];
	GLuint iSpotTexture;
	GLuint iHaloTexture;
	GLuint iSpotFrontTexture;
	GLuint iRaysTexture;
	} ;

typedef struct _CDAnimationGridNode {
	gdouble x, y;
	gdouble vx, vy;
	gdouble fx, fy;
	} CDAnimationGridNode;

typedef struct _CDAnimationData {
	gdouble fRotationSpeed;
	gdouble fRotationAngle;
	gdouble fRotationBrake;
	gdouble fAdjustFactor;
	gboolean bRotationBeginning;
	
	gdouble fIconOffsetY;
	gdouble fRadiusFactor;
	gdouble fHaloRotationAngle;
	CairoParticleSystem *pRaysSystem;
	
	gboolean bIsWobblying;
	CDAnimationGridNode gridNodes[4][4];
	GLfloat pCtrlPts[4][4][3];
	} CDAnimationData;

#endif