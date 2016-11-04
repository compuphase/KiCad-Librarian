/******************************************************************
*
* This file is copied (with minor adaptations) from X3DBrowserFunc.cpp
* which is part of CyberX3D for C++. This library is distributed
* under the 3-clause BSD license.
*
*	Copyright (C) Satoshi Konno 1996-2003
*
******************************************************************/

#if !defined NO_3DMODEL

#include <wx/glcanvas.h>
#include "vrmlsupport.h"

using namespace CyberX3D;

static int gnLights;
static PointLightNode headLight;
static float clearColor[] = {0.0f, 0.0f, 0.0f};

void SetSceneGraphBackground(unsigned long rgb)
{
    clearColor[0] = ((rgb >> 0) & 0xff) / 255.0f;
    clearColor[1] = ((rgb >> 8) & 0xff) / 255.0f;
    clearColor[2] = ((rgb >> 16) & 0xff) / 255.0f;
}

static void PushLightNode(LightNode *lightNode)
{
	if (!lightNode->isOn())
		return;

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);

	if (nMaxLightMax < gnLights) {
		gnLights++;
		return;
	}

	float	color[4];
	float	pos[4];
	float	attenuation[3];
	float	direction[3];
	float	intensity;

	if (lightNode->isPointLightNode()) {

		PointLightNode *pLight = (PointLightNode *)lightNode;

		glEnable(GL_LIGHT0+gnLights);

		pLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		pLight->getColor(color);
		intensity = pLight->getIntensity();
		color[0] *= intensity;
		color[1] *= intensity;
		color[2] *= intensity;
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		pLight->getLocation(pos); pos[3] = 1.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		direction[0] = 0.0f; direction[0] = 0.0f; direction[0] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, 180.0f);

		pLight->getAttenuation(attenuation);
		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, attenuation[0]);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, attenuation[1]);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, attenuation[2]);

		gnLights++;
	}
	else if (lightNode->isDirectionalLightNode()) {

		DirectionalLightNode *dLight = (DirectionalLightNode *)lightNode;

		glEnable(GL_LIGHT0+gnLights);

		dLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		dLight->getColor(color);
		intensity = dLight->getIntensity();
		color[0] *= intensity;
		color[1] *= intensity;
		color[2] *= intensity;
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		dLight->getDirection(pos); pos[3] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		direction[0] = 0.0f; direction[0] = 0.0f; direction[0] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, 180.0f);

		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, 1.0);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, 0.0);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, 0.0);

		gnLights++;
	}
	else if (lightNode->isSpotLightNode()) {

		SpotLightNode *sLight = (SpotLightNode *)lightNode;

		glEnable(GL_LIGHT0+gnLights);

		sLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		sLight->getColor(color);
		intensity = sLight->getIntensity();
		color[0] *= intensity;
		color[1] *= intensity;
		color[2] *= intensity;
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		sLight->getLocation(pos); pos[3] = 1.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		sLight->getDirection(direction);
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);

		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, sLight->getCutOffAngle());

		sLight->getAttenuation(attenuation);
		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, attenuation[0]);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, attenuation[1]);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, attenuation[2]);

		gnLights++;
	}
}

static void PopLightNode(LightNode *lightNode)
{
	if (!lightNode->isOn())
		return;

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);

	gnLights--;

	if (gnLights < nMaxLightMax)
		glDisable(GL_LIGHT0+gnLights);
}

static void DrawShapeNode(SceneGraph *sg, ShapeNode *shape, int drawMode)
{
	glPushMatrix ();

	/////////////////////////////////
	//	Appearance(Material)
	/////////////////////////////////

	float	color[4];
	color[3] = 1.0f;

	AppearanceNode			*appearance = shape->getAppearanceNodes();
	MaterialNode			*material = NULL;
	ImageTextureNode		*imgTexture = NULL;
	TextureTransformNode	*texTransform = NULL;

	bool				bEnableTexture = false;

	if (appearance) {

		// Texture Transform
		TextureTransformNode *texTransform = appearance->getTextureTransformNodes();
		if (texTransform) {
			float texCenter[2];
			float texScale[2];
			float texTranslation[2];
			float texRotation;

			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();

			texTransform->getTranslation(texTranslation);
			glTranslatef(texTranslation[0], texTranslation[1], 0.0f);

			texTransform->getCenter(texCenter);
			glTranslatef(texCenter[0], texCenter[1], 0.0f);

			texRotation = texTransform->getRotation();
			glRotatef(0.0f, 0.0f, 1.0f, texRotation);

			texTransform->getScale(texScale);
			glScalef(texScale[0], texScale[1], 1.0f);

			glTranslatef(-texCenter[0], -texCenter[1], 0.0f);
		}
		else {
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glTranslatef(0.0f, 0.0f, 1.0f);
		}

		glMatrixMode(GL_MODELVIEW);

		// Texture
		imgTexture = appearance->getImageTextureNodes();
		if (imgTexture && drawMode == OGL_RENDERING_TEXTURE) {

			int width = imgTexture->getWidth();
			int height = imgTexture->getHeight();
			RGBAColor32 *color = imgTexture->getImage();

			if (0 < width && 0 < height && color != NULL)
				bEnableTexture = true;

			if (bEnableTexture == true) {
				if (imgTexture->hasTransparencyColor() == true) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else
					glDisable(GL_BLEND);

				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glBindTexture(GL_TEXTURE_2D, imgTexture->getTextureName());

				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

				glEnable(GL_TEXTURE_2D);

				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
				glEnable(GL_COLOR_MATERIAL);
			}
			else {
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_COLOR_MATERIAL);
			}
		}
		else {
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_COLOR_MATERIAL);
		}

		// Material
		material = appearance->getMaterialNodes();
		if (material) {
			float	ambientIntesity = material->getAmbientIntensity();

			material->getDiffuseColor(color);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

			material->getSpecularColor(color);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);

			material->getEmissiveColor(color);
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);

			material->getDiffuseColor(color);
			color[0] *= ambientIntesity;
			color[1] *= ambientIntesity;
			color[2] *= ambientIntesity;
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);

			glMateriali(GL_FRONT, GL_SHININESS, (int)(material->getShininess()*128.0));
		}

	}

	if (!appearance || !material) {
		color[0] = 0.8f; color[1] = 0.8f; color[2] = 0.8f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
		color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);
		color[0] = 0.8f*0.2f; color[1] = 0.8f*0.2f; color[2] = 0.8f*0.2f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
		glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, (int)(0.2*128.0));
	}

	if (!appearance || !imgTexture || drawMode != OGL_RENDERING_TEXTURE) {
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}

	/////////////////////////////////
	//	Transform
	/////////////////////////////////

	float	m4[4][4];
	shape->getTransformMatrix(m4);
	glMultMatrixf((float *)m4);

	glColor3f(1.0f, 1.0f, 1.0f);

	/////////////////////////////////
	//	Geometry3D
	/////////////////////////////////

	Geometry3DNode *gnode = shape->getGeometry3D();
	if (gnode) {
		if (0 < gnode->getDisplayList())
			gnode->draw();
	}

	ShapeNode *selectedShapeNode = sg->getSelectedShapeNode();
	if (gnode && selectedShapeNode == shape) {
		float	bboxCenter[3];
		float	bboxSize[3];
		gnode->getBoundingBoxCenter(bboxCenter);
		gnode->getBoundingBoxSize(bboxSize);

		glColor3f(1.0f, 1.0f, 1.0f);
		glDisable(GL_LIGHTING);
//		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);

		glBegin(GL_LINES);
		int x, y, z;
		for (x=0; x<2; x++) {
			for (y=0; y<2; y++) {
				float point[3];
				point[0] = (x==0) ? bboxCenter[0] - bboxSize[0] : bboxCenter[0] + bboxSize[0];
				point[1] = (y==0) ? bboxCenter[1] - bboxSize[1] : bboxCenter[1] + bboxSize[1];
				point[2] = bboxCenter[2] - bboxSize[2];
				glVertex3fv(point);
				point[2] = bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
			}
		}
		for (x=0; x<2; x++) {
			for (z=0; z<2; z++) {
				float point[3];
				point[0] = (x==0) ? bboxCenter[0] - bboxSize[0] : bboxCenter[0] + bboxSize[0];
				point[1] = bboxCenter[1] - bboxSize[1];
				point[2] = (z==0) ? bboxCenter[2] - bboxSize[2] : bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
				point[1] = bboxCenter[1] + bboxSize[1];
				glVertex3fv(point);
			}
		}
		for (y=0; y<2; y++) {
			for (z=0; z<2; z++) {
				float point[3];
				point[0] = bboxCenter[0] - bboxSize[0];
				point[1] = (y==0) ? bboxCenter[1] - bboxSize[1] : bboxCenter[1] + bboxSize[1];
				point[2] = (z==0) ? bboxCenter[2] - bboxSize[2] : bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
				point[0] = bboxCenter[0] + bboxSize[0];
				glVertex3fv(point);
			}
		}
		glEnd();

		glEnable(GL_LIGHTING);
//		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
	}

	glPopMatrix();
}


static void DrawNode(SceneGraph *sceneGraph, Node *firstNode, int drawMode)
{
	if (!firstNode)
		return;

	Node	*node;

	for (node = firstNode; node; node=node->next()) {
		if (node->isLightNode())
			PushLightNode((LightNode *)node);
	}

	for (node = firstNode; node; node=node->next()) {
		if (node->isShapeNode())
			DrawShapeNode(sceneGraph, (ShapeNode *)node, drawMode);
		else
			DrawNode(sceneGraph, node->getChildNodes(), drawMode);
	}

	for (node = firstNode; node; node=node->next()) {
		if (node->isLightNode())
			PopLightNode((LightNode *)node);
	}
}

void DrawSceneGraph(SceneGraph *sg, int drawMode, float AngleX, float AngleZ)
{
	/////////////////////////////////
	//	Headlight
	/////////////////////////////////

	NavigationInfoNode *navInfo = sg->getNavigationInfoNode();
	if (navInfo == NULL)
		navInfo = sg->getDefaultNavigationInfoNode();

	if (navInfo->getHeadlight()) {
		float	location[3];
		ViewpointNode *view = sg->getViewpointNode();
		if (view == NULL)
			view = sg->getDefaultViewpointNode();
		view->getPosition(location);
		headLight.setLocation(location);
		headLight.setAmbientIntensity(0.3f);
		headLight.setIntensity(0.7f);
		sg->addNode(&headLight);
	}

	/////////////////////////////////
	//	Viewpoint
	/////////////////////////////////

	ViewpointNode *view = sg->getViewpointNode();
	if (view == NULL)
		view = sg->getDefaultViewpointNode();

#if 0   //??? default viewport
	if (view) {
		GLint	viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		UpdateViewport(sg, viewport[2], viewport[3]);
	}
#endif

	/////////////////////////////////
	//	Rendering
	/////////////////////////////////

	glEnable(GL_DEPTH_TEST);
	switch (drawMode) {
	case OGL_RENDERING_WIRE:
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		break;
	default:
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	glEnable(GL_LIGHTING);
	glShadeModel (GL_SMOOTH);

	/////////////////////////////////
	//	Background
	/////////////////////////////////

	BackgroundNode *bg = sg->getBackgroundNode();
	if (bg != NULL) {
		if (0 < bg->getNSkyColors())
			bg->getSkyColor(0, clearColor);
	}

	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	if (!view)
		return;

	/////////////////////////////////
	//	Viewpoint Matrix
	/////////////////////////////////

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float	m4[4][4];
	view->getMatrix(m4);
	glMultMatrixf((float *)m4);

    glRotatef(AngleX, 1.0, 0.0, 0.0);
    glRotatef(AngleZ, 0.0, 1.0, 0.0);
    glRotatef(30, 0.0, 0.0, 1.0);

	/////////////////////////////////
	//	Light
	/////////////////////////////////

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);
	for (int n = 0; n < nMaxLightMax; n++)
		glDisable(GL_LIGHT0+n);
	gnLights = 0;

	/////////////////////////////////
	//	General Node
	/////////////////////////////////

	DrawNode(sg, sg->getNodes(), drawMode);

	/////////////////////////////////
	//	Headlight
	/////////////////////////////////

	headLight.remove();

	/////////////////////////////////
	//	glFlush
	/////////////////////////////////

	glFlush();
}

#endif /* !defined NO_3DMODEL */
