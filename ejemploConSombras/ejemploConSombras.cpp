/*Actualización 05/02/2026 para el uso de github :D*/
/*Segunda actualizacion implementacion de caracteristicas de debugger
la idea es usar las siguientes caraceristicas

(F1) Culling (Recorte de caras traseras)
	- Teoricamente es esto: 
		¿'para que dibujar la espalda del cubo si no la estamos viendo'?
		GL_CULL_FACE le dice a OpenGL: "si un triangulo apunta hacia atrás, ignoralo"
		Esto es vital para ahorrar memoria

		-- el efecto visual: si nos metemos dentro del cubo, este desaparecerá. porque estarás viendo las 
		   caras "por detras".
(F2) Depth Test (prueba de profundidad): 
	- Teoria: Es el Z-Buffer. Decide qué pixel está delante de cuál.
	- El efecto visual: si lo apagamos, veremos el caos. El cubi se dibujará en el orden en que fue programado
	  no en el orden de distancoa. Las Patas traseras podrían verse encima de las delanteras.

(F3) Polygon Mode (Modo Alambre) : 
	- Teoría: Le dice a la GPU: #no rellenes los triángulos con color, solo dibuja las lineas de los bordes#
		-- El efecto es que nuestro cubo loco de colores solidos se notara poco, pero si tuvieramos
		   un cubo multicolor en una sola cara, veriamos un degradado suave.
(F4) Shading Model (suavizado):
	-Teoria: GL_FLAT (un color por cara) vs GL_SMOOTH (mezcla los colores de los vértices)
		-- efecto visual esperado: En nuestro cubo de colores sólidos se notará poco, pero si tuvieramos 
		un cubo multicolor en una sola cara, veriamos un degradado suavecito. 

Nueva implementacion 08/02/2026
	- De momento nos encargaremos de darle control de "altura eje Y" al foco puntual 
	  para que la luz pueda asecender o descender. 
	  para ello pensamos en la siguiente logica: 
		- si pulsamos el numero '8' entondes incrementaremos la altura a la cual se encuentra el foco
		- si pulsamos el numero '2' entonces decrementa remos la altura a la cual se encuentra el foco

Actualizacion: 
	se ha detectado un fallo respecto a un bugg de acercamiento, sucede cuado el foco 
	puntual toca el cubo, en la deformacion de la sobra se rompe la impresion de la imagen
		- para evitar eso es necesario hacer una correccion de proximidad.
		  Usaremos un limitador de modo que el foco no llegue a tocar al cubo.

		- Paraa poder evidenciar la esfera, usaremos la tecla 'f', la cual aactivara o descativara
		  la esfera de referencia
		- Tambien implementaremos un crecimiento del cubo variable, la cual podremos variar 
		  dependiendo las necesidades
			- esta idea tambien debe de afectar la distancia focal para evitar 
			  que al crecer la esfera encapsule el foco puntual probocando infinidad de buggs
			- usaremos variables globales para esta accion
			- nota es posible no dibujar el campo de fuerza y aun asi funsionaria dado que 
			  solo es una condicion 
			  -- pero para que quede mas claro es mejor usar una esfera de referencia

*/
#include <math.h>
#include <stdio.h>
#include"glut.h"

// ---------------- VARIABLES GLOBALES ---------------- //
/*
Explicaciones de las cariables globales
las variables globales son "la memoria" del programa. 
como openGL es una m+aquina de estados, necesitamos guardar cómo está el mundo entre cada parpadeo
de la pantalla.
	*camDist, camAngleX, camAngleY : son las coordenadas Esféricas. En lugar de guardar (x,y,z) de la cámara
	guardamos "que tan lejos esta" (radio) y "hacia dónde gira" (angulos). Luego usamos trigonometria(sin,cos)
	para calcular la (x,y,z) real

	*lighpost[]: La posicion de la luz. El cuarto numero es vital #1.0f# 
	 -- si fuera 0.0f, seria luz direccional como el sol
	 -- si es 1.0f, es luz puntual "como de foquito", la cual permite deformación de perspectiva
		de la sombra
	*floorPlane: La matematica del piso
		* La ecuacion Ax + Bx + Cz + D = 0
		* el vector de este plano lo dejamos como {0,1,0,2}. Lo que se traduce como 0x + 1y + 0z + 2 =0
		lo que resulta que y = -2 por lo cual el plano se encuentra a una altura de -2.

*/

// 1. Control de CÁMARA (Sistema Polar para efecto Drone)
float camDist = 15.0f;  // Distancia inicial
float camAngleX = 0.0f; // Rotación horizontal
float camAngleY = 0.0f; // Rotación vertical
int lastMouseX, lastMouseY; // Para rastrear el arrastre del mouse
//bool isDragging = false;

float escalaCubo = 1.0f; // 1.0 es el tamaño base, dependiendo del cambio respondera la escala del cubo
bool bMostrarEsfera = false; // bandera de activacion de campo, cambiara al pulsar f

int interactionMode = 0; // 0 = Nada, 1 = Moviendo CÁMARA, 2 = Moviendo CUBO

bool animacionActiva = false; // El interruptor inicia apagado

// 2. Control de LUZ (El Foco)
// Posición inicial: Arriba y a la izquierda , declaramos.
float lightPos[4] = { -4.0f, 4.0f, 4.0f, 1.0f };

// Posisión inicial altura eje Y del foco
float alturaLuz = 4.0f;

// 3. Definición del PISO (Plano matemático)
// Ecuación: 0x + 1y + 0z + 2 = 0  (Piso en Y = -2.0)
GLfloat floorPlane[4] = { 0.0f, 1.0f, 0.0f, 2.0f };

static GLfloat theta[] = { 0.0f, 0.0f, 0.0f }; // Ángulos de rotación del cubo

// nuevas variables para la implementacion de las funciones debugg 05/02/2026
bool bCull = false; //estara destinado para usarlo con F1: ocultar las caras traseras
bool bDepth = true; // enfocado en la profundidad con F2: Z-buffer
bool bWireframe = false; //para el modo alambre : F3
bool bSmooth = false; // para el efecto de suavizado : F4

// ---------------- GEOMETRÍA DEL CUBO (LEGACY) ---------------- //
/*
El esqueleto : LA construcción del Cubo (cara 0 a cara 6)
Aqui ocurre algo magico, llamado Modelado Jerárquico. No dibujaremos un cubo de golpe; dibujaremos una cara
y la clonamos moviendola :D
*/
void cara0() {
	// Si el modo Suavizado (F4) está activo...
	if (bSmooth) {
		glShadeModel(GL_SMOOTH);
		glBegin(GL_POLYGON);
		glNormal3f(0.0f, 0.0f, 1.0f);

		// HACK DE DEBUGGING: 
		// Forzamos colores diferentes en cada esquina para ver el degradado
		glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(1., 1., 0);   // Rojo
		glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(1., -1., 0);  // Verde
		glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(-1., -1., 0); // Azul
		glColor3f(1.0f, 1.0f, 0.0f); glVertex3f(-1., 1., 0);  // Amarillo
		glEnd();
	}
	else {
		// Si F4 está apagado, funcionamos normal (Color Sólido FLat)
		glShadeModel(GL_FLAT);
		glBegin(GL_POLYGON);
		glNormal3f(0.0f, 0.0f, 1.0f);
		// Aquí NO ponemos glColor, así que usará el color que definió colorcube()
		glVertex3f(1., 1., 0);
		glVertex3f(1., -1., 0);
		glVertex3f(-1., -1., 0);
		glVertex3f(-1., 1., 0);
		glEnd();
	}
}

/*
aquí te describo el funcionamiento de cada cosita dentro del metodo cara1
	- glPushMatrix(): guarda la posicion actual del mundo
	- glTranslate(0,0,1): mueve el lápiz 1 metro hacia el frente.
	- cara0(); Dibujara la cara base.
	- glPopMatrix(): regresa el lápiz a donde estaba antes de moverlo
	- Resultado: Dibuja la tapa frontal del cubo

*/
void cara1() {
	glPushMatrix();
	glTranslatef(0., 0., 1.);
	cara0();
	glPopMatrix();
}
/*
desde aqui hasta la cara6() se repite el patron anterior, pero rotando 90 grados para colocar las tapas laterales
arriba y abajo.

ahora si queremos cambiar el tamaño del cubo solo modificariamos el tamaño de la cara base, es decir cara0
*/
void cara2() {
	glPushMatrix();
	glRotatef(90., 1., 0., 0.);
	cara1();
	glPopMatrix();
}
void cara3() {
	glPushMatrix();
	glRotatef(90., 1., 0., 0.);
	cara2();
	glPopMatrix();
}
void cara4() {
	glPushMatrix();
	glRotatef(90., 1., 0., 0.);
	cara3();
	glPopMatrix();
}
void cara5() {
	glPushMatrix();
	glRotatef(90., 0., 1., 0.);
	cara1();
	glPopMatrix();
}
void cara6() {
	glPushMatrix();
	glRotatef(-90., 0., 1., 0.);
	cara1();
	glPopMatrix();
}

// Cubo CON color (Para el objeto real)
void colorcube(void) {
	glColor3f(0.5, 0., 0.); cara1();
	glColor3f(0., 0.5, 0.); cara2();
	glColor3f(0., 0., 0.5); cara3();
	glColor3f(0., 0.5, 0.5); cara4();
	glColor3f(0.5, 0.5, 0.); cara5();
	glColor3f(0.5, 0., 0.5); cara6();
}

// Cubo SIN color (Para la sombra negra)
void colorcube_sin_color(void) {
	cara1();
	cara2();
	cara3();
	cara4();
	cara5();
	cara6();
}

// ---------------- FUNCIONES AUXILIARES ---------------- //

// Dibuja el piso gris y una rejilla para referencia
void DibujarPiso() {
	// Superficie sólida
	glColor3f(0.6f, 0.6f, 0.6f);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glVertex3f(-10.0f, -2.0f, -10.0f);
	glVertex3f(-10.0f, -2.0f, 10.0f);
	glVertex3f(10.0f, -2.0f, 10.0f);
	glVertex3f(10.0f, -2.0f, -10.0f);
	glEnd();
}

// Dibuja una esfera amarilla donde está la luz (para saber qué movemos)
// no es necesario actualizar esta funcsion para agregar la caracteristica de control de foco
//    dado que funciona de manera dinamica usando lightPost
void DibujarFoco() {
	glPushMatrix();
	glTranslatef(lightPos[0], lightPos[1], lightPos[2]);
	glColor3f(1.0f, 1.0f, 0.0f); // Amarillo
	glutSolidSphere(0.2, 10, 10);
	glPopMatrix();
}

// MATEMÁTICA DE SOMBRAS (Planar Shadow Matrix)
// Fuente: Diapositiva 13 del curso
/*
Esta es la funcion más perrona de todas. Es algebra lineal pura "proyeccion planar"
vPlaneEquation : Donde esta el piso jefesito
vLightPos : Donde esta la luz bro
1.- dot (producto punto) : vPlaneEquation * vLightPos
	- calcula el ángulo y distancia entre la luz y el plano. Nos dice si la luz esta "viendo" al plano
		de frente o de lado.
2.- La matriz (destMat) : 
	- Lo que hace esta matriz es crear una transformación que colapsa la dimensión "Y" (altura) basandose
	  en la linea que contenga la luz con el vertice. 
	  basicamente intercepta el rayo de luz con el plano

Output: 
	- Devuelve una matrix de 4x4 que, al multiplicarla por cualquier objeto 3D, lo aplasta contra el suelo
	como si fuera una estampa.
*/
void gltMakeShadowMatrix(GLfloat vPlaneEquation[], GLfloat vLightPos[], GLfloat destMat[]) {
	GLfloat dot;
	dot = vPlaneEquation[0] * vLightPos[0] + vPlaneEquation[1] * vLightPos[1] +
		vPlaneEquation[2] * vLightPos[2] + vPlaneEquation[3] * vLightPos[3];

	destMat[0] = dot - vLightPos[0] * vPlaneEquation[0];
	destMat[4] = 0.0f - vLightPos[0] * vPlaneEquation[1];
	destMat[8] = 0.0f - vLightPos[0] * vPlaneEquation[2];
	destMat[12] = 0.0f - vLightPos[0] * vPlaneEquation[3];

	destMat[1] = 0.0f - vLightPos[1] * vPlaneEquation[0];
	destMat[5] = dot - vLightPos[1] * vPlaneEquation[1];
	destMat[9] = 0.0f - vLightPos[1] * vPlaneEquation[2];
	destMat[13] = 0.0f - vLightPos[1] * vPlaneEquation[3];

	destMat[2] = 0.0f - vLightPos[2] * vPlaneEquation[0];
	destMat[6] = 0.0f - vLightPos[2] * vPlaneEquation[1];
	destMat[10] = dot - vLightPos[2] * vPlaneEquation[2];
	destMat[14] = 0.0f - vLightPos[2] * vPlaneEquation[3];

	destMat[3] = 0.0f - vLightPos[3] * vPlaneEquation[0];
	destMat[7] = 0.0f - vLightPos[3] * vPlaneEquation[1];
	destMat[11] = 0.0f - vLightPos[3] * vPlaneEquation[2];
	destMat[15] = dot - vLightPos[3] * vPlaneEquation[3];
}

// ---------------- CONTROL DE USUARIO (INPUTS) ---------------- //

// Teclado Normal: W/S para Zoom
void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 'w': case 'W': // Zoom In
		if (camDist > 3.0f) camDist -= 0.5f;
		break;
	case 's': case 'S': // Zoom Out
		camDist += 0.5f;
		break;
	case 27: // ESC para salir
		exit(0);
		break;
	case '8' : // aumentamos la altura del foco
		alturaLuz += 0.5f; // 0.5 por sincronia con el acercamiento y alejamiento
		lightPos[1]= alturaLuz;
		break;
	case '2': //decrementamos la altura del foco
		alturaLuz -= 0.5;
		lightPos[1] = alturaLuz;
		break;
	}
	glutPostRedisplay();
}

// Flechas: Mover la Luz
void specialKeys(int key, int x, int y) {
	float step = 0.5f;
	switch (key) {
		case GLUT_KEY_UP:    lightPos[2] -= step; break; // Z Atrás
		case GLUT_KEY_DOWN:  lightPos[2] += step; break; // Z Adelante
		case GLUT_KEY_LEFT:  lightPos[0] -= step; break; // X Izquierda
		case GLUT_KEY_RIGHT: lightPos[0] += step; break; // X Derecha

		//Controles de estado (modo DEBUGG)
		case GLUT_KEY_F1:
			bCull = !bCull;
			//Recuerda que openGl usa "Counter Clockwise (CCW) por defecto para el frene"
			if (bCull) glEnable(GL_CULL_FACE);
			else glDisable(GL_CULL_FACE);
			break;
		case GLUT_KEY_F2:
			bDepth = !bDepth;
			if (bDepth) glEnable(GL_DEPTH_TEST);
			else glDisable(GL_DEPTH_TEST);
			break;
		case GLUT_KEY_F3:
			bWireframe = !bWireframe;
			if (bWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		case GLUT_KEY_F4:
			bSmooth = !bSmooth;
			// recuerda que el cambio real solo es neceario dentro de la cara 0 al redibujar :D
			break;
	}

	glutPostRedisplay();
}

// Mouse: Click para empezar a arrastrar
void mouseButton(int button, int state, int x, int y) {
	// Guardamos la posición inicial siempre que se presiona un botón
	if (state == GLUT_DOWN) {
		lastMouseX = x;
		lastMouseY = y;
	}

	// Lógica de selección de MODO
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			interactionMode = 1; // Modo 1: Mover Cámara (Drone)
		}
		else {
			interactionMode = 0; // Soltar: Dejar de mover
		}
	}
	else if (button == GLUT_RIGHT_BUTTON) {
		if (state == GLUT_DOWN) {
			interactionMode = 2; // Modo 2: Girar CUBO (Trackball)
		}
		else {
			interactionMode = 0; // Soltar: Dejar de mover
		}
	}
}

void mouseMove(int x, int y) {
	// Si no estamos haciendo nada, salimos
	if (interactionMode == 0) return;

	// Calcular cuánto se movió el mouse desde el último cuadro
	int dx = x - lastMouseX;
	int dy = y - lastMouseY;

	// CASO 1: MOVEMOS LA CÁMARA (Clic Izquierdo)
	if (interactionMode == 1) {
		camAngleX += dx * 0.5f;
		camAngleY += dy * 0.5f;

		// Limites para que la cámara no de vueltas locas verticales
		if (camAngleY > 89.0f) camAngleY = 89.0f;
		if (camAngleY < -89.0f) camAngleY = -89.0f;
	}

	// CASO 2: GIRAMOS EL CUBO (Clic Derecho) -> ¡Lo que tú pediste!
	if (interactionMode == 2) {
		// Nota la matemática cruzada:
		// Mover mouse en X (horizontal) -> Gira el cubo en eje Y (Vertical)
		// Mover mouse en Y (vertical) -> Gira el cubo en eje X (Horizontal)
		theta[1] += dx * 0.5f;
		theta[0] += dy * 0.5f;
	}

	// Actualizamos la posición "anterior" para el siguiente cuadro
	lastMouseX = x;
	lastMouseY = y;

	// ¡Obligatorio! Pedir a OpenGL que dibuje la escena con los nuevos datos
	glutPostRedisplay();
}

// ---------------- RENDERIZADO PRINCIPAL ---------------- //
/*
Aqui es donde ocurre la magia del orden (Algoritmo del Pintor).
Paso A: El lienzo glClear(...), esto borra lo que había en el cuadro 
anterior. Si no hacemos esto, existira un efecto rastro de todo
lo que se mueve "como cuando quitamos el SO instalado en algun 
medio extraible"
Puedes hacer la prueba comentando glClear(...) y ver que es lo que sucede


actualizacion para la implementacion de las teclas especiales
// 1. Preguntamos: "¿Cómo estaba el interruptor ANTES de la sombra?"
// 2. Apagamos OBLIGATORIAMENTE para la sombra (la sombra siempre necesita esto off)
// 3. Restauramos: "Déjalo como lo encontraste"

*/
void display(void){
	//Limpieza
	//curiosamente si omito glClear se borra todo
	//y solo aparece la ventana en color negro
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	// 1. CÁMARA (Drone) - Se mueve con Clic IZQUIERDO
	float radX = camAngleX * 3.14159f / 180.0f;
	float radY = camAngleY * 3.14159f / 180.0f;



	gluLookAt(camDist * sin(radX) * cos(radY) , camDist * sin(radY) , camDist * cos(radX) * cos(radY),
				0,0,0,  0,1,0);

	// 2. DIBUJAR FOCO Y PISO
	DibujarFoco();
	DibujarPiso();

	// 3. SOMBRA
		// Nota experta: Aunque F2 apague el Depth Test globalmente,
		// la sombra SIEMPRE necesita Depth Test apagado para dibujarse.
	GLboolean profundidadEstabaActiva = glIsEnabled(GL_DEPTH_TEST);

	/*
	glDisable(GL_DEPTH_TEST)
		- El problema: la sombra matemática cae exactamente en el 
		  mismo pixel que el piso (Y = -2). La computadora no sabe 
		  cuál dibujar primero y parpadea efecto #Z-Fighting#
		- La solución: "Desactivar la prueba de profundidad"
		  le ordenamos a OpenGL que pinte la sombra encima de lo
		  que sea que haya ahí (en este caso el piso), sin preguntar
		  si esta mas cerca o mas lejos.
	*/
	glDisable(GL_DEPTH_TEST); // Evitar parpadeo


	glPushMatrix();
		GLfloat sombraMat[16];
		gltMakeShadowMatrix(floorPlane, lightPos, sombraMat);

	/*
	- Activamos la trituradora. Todo lo que dibujamos después de esta línea
	será aplastado
		glMultMatrix y le pasamos 
	*/
		glMultMatrixf(sombraMat); // 1. Aplastamos

		// --- CORRECCIÓN AQUÍ: Rotación Dinámica para la Sombra ---
		glRotatef(theta[0], 1.0, 0.0, 0.0);
		glRotatef(theta[1], 0.0, 1.0, 0.0);
		// ---------------------------------------------------------

		glColor3f(0.0f, 0.0f, 0.0f);
		colorcube_sin_color();
	glPopMatrix();




	// Restauramos el estado de profundidad según lo que diga F2
	if (profundidadEstabaActiva) glEnable(GL_DEPTH_TEST);

	// 4. DIBUJAR OBJETO REAL (Debe obedecer al mouse)
	glPushMatrix();
	// --- CORRECCIÓN AQUÍ: Quitamos el "30.0" fijo y ponemos tus variables ---
		glRotatef(theta[0], 1.0, 0.0, 0.0);
		glRotatef(theta[1], 0.0, 1.0, 0.0);
		colorcube();
	glPopMatrix();
	// -----------------------------------------------------------------------
	glutSwapBuffers();
}

void myReshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, (GLfloat)w / (GLfloat)h, 1.0f, 100.0f); // Perspectiva real
	glMatrixMode(GL_MODELVIEW);
}

void spinCube()
{
	// Solo giramos si el interruptor está encendido
	if (animacionActiva) {
		theta[0] += 1.0f; // Gira en X
		theta[1] += 0.5f; // Gira en Y un poco más lento
		if (theta[0] > 360.0) theta[0] -= 360.0;
		if (theta[1] > 360.0) theta[1] -= 360.0;

		glutPostRedisplay(); // ¡Dibuja de nuevo!
	}
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 600);
	glutCreateWindow("Proyecto Graficas: Sombras + Camara Drone + debugger F1-F4");

	glutReshapeFunc(myReshape);
	glutDisplayFunc(display);

	// Registro de controles
	glutKeyboardFunc(keyboard); // Teclas W, S
	glutSpecialFunc(specialKeys); // Flechas
	glutMouseFunc(mouseButton); // Click
	glutMotionFunc(mouseMove);  // Arrastre
	//glutIdleFunc(spinCube); // Esta función se ejecuta siempre que la compu descansa

	//configuracion inicial
	glEnable(GL_DEPTH_TEST);

	// IMPORTANTE: Definir el orden de los vértices para que F1 funcione
	glFrontFace(GL_CCW); // Counter Clock-Wise (Sentido antihorario es el frente)

	glutMainLoop();
	return 0;
}