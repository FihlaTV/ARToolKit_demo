// multitest2.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>
#include <AR/ar.h>
char *vconf = "../Data/WDM_camera_flipV.xml";
int             xsize, ysize;
int             thresh = 100;
int             count = 0;
char           *cparam_name = "../Data/camera_para.dat";
ARParam         cparam;


#define  OBJ1_PATT_NAME    "../Data/patt.hiro"
#define  OBJ2_PATT_NAME    "../Data/patt.kanji"
#define  OBJ1_SIZE         80.0
#define  OBJ2_SIZE         80.0
#define  OBJ1_MODEL_ID      1
#define  OBJ2_MODEL_ID      2
typedef struct {
	char    *patt_name;
	int     patt_id;
	int     model_id;
	int     visible;
	double  width;
	double  center[2];
	double  trans[3][4];
} OBJECT_T;

OBJECT_T   object[2] = {
	{ OBJ1_PATT_NAME, -1, OBJ1_MODEL_ID, 0, OBJ1_SIZE,{ 0.0,0.0 } },
	{ OBJ2_PATT_NAME, -1, OBJ2_MODEL_ID, 0, OBJ2_SIZE,{ 0.0,0.0 } }
};

static void   init(void);
static void   cleanup(void);
static void   mainLoop(void);
static void   draw(int object, double trans[3][4]);
int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	init();
	arVideoCapStart();
	argMainLoop(NULL, NULL, mainLoop);
	return (0);
}
static void mainLoop(void)
{
	ARUint8         *dataPtr;
	ARMarkerInfo    *marker_info;
	int             marker_num;
	int             i, j, k;
	if ((dataPtr = (ARUint8 *)arVideoGetImage()) == NULL) {
		arUtilSleep(2);
		return;
	}
	if (count == 0) arUtilTimerReset();
	count++;
	argDrawMode2D();
	argDispImage(dataPtr, 0, 0);
	if (arDetectMarker(dataPtr, thresh, &marker_info, &marker_num) < 0) {
		cleanup();
		exit(0);
	}
	arVideoCapNext();
	argDrawMode3D();
	argDraw3dCamera(0, 0);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);
	for (i = 0; i < 2; i++) {
		k = -1;
		for (j = 0; j < marker_num; j++) {
			if (object[i].patt_id == marker_info[j].id) {
				if (k == -1) k = j;
				else if (marker_info[k].cf < marker_info[j].cf) k = j;
			}
		}
		object[i].visible = k;
		if (k >= 0) {
			arGetTransMat(&marker_info[k],
				object[i].center, object[i].width,
				object[i].trans);
			draw(object[i].model_id, object[i].trans);
		}
	}
	argSwapBuffers();
	//if (object[0].visible >= 0
	//	&& object[1].visible >= 0) {
	//	double  wmat1[3][4], wmat2[3][4];
	//	arUtilMatInv(object[0].trans, wmat1);
	//	arUtilMatMul(wmat1, object[1].trans, wmat2);
	//	for (j = 0; j < 3; j++) {
	//		for (i = 0; i < 4; i++) printf("%8.4f ", wmat2[j][i]);
	//	}
	//}
}
static void init(void)
{
	ARParam  wparam;
	int      i;
	arVideoOpen(vconf);
	arVideoInqSize(&xsize, &ysize);
	arParamLoad(cparam_name, 1, &wparam);
	arParamChangeSize(&wparam, xsize, ysize, &cparam);
	arInitCparam(&cparam);
	arParamDisp(&cparam);

	for (i = 0; i < 2; i++) {
		object[i].patt_id = arLoadPatt(object[i].patt_name);
	}
	argInit(&cparam, 1.0, 0, 0, 0, 0);
}

static void cleanup(void)
{
	arVideoCapStop();
	arVideoClose();
	argCleanup();
}

static void draw(int object, double trans[3][4])
{
	double    gl_para[16];
	GLfloat   mat_ambient[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_flash[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_flash_shiny[] = { 50.0 };
	GLfloat   light_position[] = { 100.0,-200.0,200.0,0.0 };
	GLfloat   ambi[] = { 0.1, 0.1, 0.1, 0.1 };
	GLfloat   lightZeroColor[] = { 0.9, 0.9, 0.9, 0.1 };

	argDrawMode3D();
	argDraw3dCamera(0, 0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	argConvGlpara(trans, gl_para);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(gl_para);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);
	glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
	glMatrixMode(GL_MODELVIEW);

	switch (object) {
	case 0:
		glTranslatef(0.0, 0.0, 25.0);
		glutSolidCube(50.0);
		break;
	case 1:
		glTranslatef(0.0, 0.0, 40.0);
		glutSolidSphere(40.0, 24, 24);
		break;
	case 2:
		glutSolidCone(25.0, 100.0, 20, 24);
		break;
	default:
		glTranslatef(0.0, 0.0, 10.0);
		glutSolidTorus(10.0, 40.0, 24, 24);
		break;
	}
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
}