 //Autor: Ognjen Milicevic
//Opis: Projektni zadatak "Bespilotnik"

#define _CRT_SECURE_NO_WARNINGS
#define CRES 30
#define SPEED 0.02//0.002


#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <map>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>


//stb_image.h je header-only biblioteka za ucitavanje tekstura.
//Potrebno je definisati STB_IMAGE_IMPLEMENTATION prije njenog ukljucivanja
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


//#include "TextRenderer.h"
#include <ft2build.h>
#include FT_FREETYPE_H

unsigned int compileShader(GLenum type, const char* source);
unsigned int createShader(const char* vsSource, const char* fsSource);
static unsigned loadImageToTexture(const char* filePath); //Ucitavanje teksture, izdvojeno u funkciju
void createMainVao(unsigned int* VAO, unsigned int* VBO, unsigned int* EBO);
void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color, unsigned int shaderId, unsigned int VAO, unsigned int VBO);
void createCircleVao(unsigned int* VAO, unsigned int* VBO, float x0, float y0, float r);
bool circlesCollide(float cx1, float cy1, float r1, float cx2, float cy2, float r2);
bool pointIsOutOfBounds(float px, float py);
int mapToPercentage(float translationFactor);
glm::vec2 normalizeToPixels(float x, float y, int screenWidth, int screenHeight);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

int percentage1, percentage2;

struct Character {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Offset to advance to next glyph
};

//void create2ndAircraftVao(unsigned int* VAO);
std::map<char, Character> Characters;
float gx0 = 0.03, gy0 = -0.80, gRestrictedZoneRadius = 0.1;
bool resizingRestrictedZone = false;
float restrictedZoneColor[4] = { 0.5, 0.1, 0.1, 0.5 };
int main(void)
{

    if (!glfwInit())
    {
        std::cout<<"GLFW Biblioteka se nije ucitala! :(\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window;
    unsigned int wWidth = 1280;
    unsigned int wHeight = 800;
    const char wTitle[] = "[Bespilotnik]";
    window = glfwCreateWindow(wWidth, wHeight, wTitle, NULL, NULL);
    
    if (window == NULL)
    {
        std::cout << "Prozor nije napravljen! :(\n";
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);

    //MOUSE CLICK CALLBACK
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (glewInit() != GLEW_OK)
    {
        std::cout << "GLEW nije mogao da se ucita! :'(\n";
        return 3;
    }


    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "fonts/arial.ttf", 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }
    FT_Set_Pixel_Sizes(face, 0, 48);


    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(wWidth), 0.0f, static_cast<float>(wHeight));
    for (unsigned char c = 0; c < 128; c++)
    {
        // load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    unsigned int mapShader = createShader("basic.vert", "map.frag");
    unsigned int filterShader = createShader("basic.vert", "filter.frag");
    unsigned int universalRectShader = createShader("basic.vert", "universalRect.frag");
    unsigned int textShader = createShader("text.vert", "text.frag");


    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader, "projection"),
        1, GL_FALSE, glm::value_ptr(projection));
    glUseProgram(0);
    //init buffers for text rendering
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //CENTER COORD
    float x0[3] = {-0.7, 0.58, 0.03};
    float y0[3] = {-0.7, -0.5, -0.80};
    float r1 = 0.02;
    float r2 = 0.02;
    float r3 = 0.1;

    unsigned int mainVAO, mainVBO, mainEBO; //VAO za sve pravougaone objekte, jer svaki ima svoj Viewport i mogu biti obicni pravougaonici koji zauzimaju ceo svoj Viewport
    unsigned int airCraftVAO[2]; //VAO za oba aviona
    unsigned int airCraftVBO[2]; //VBO za oba aviona da bi kasnije mogao da ih ispraznim
    unsigned int restrictedZoneVAO; //Zabranjena zona VAO
    unsigned int restrictedZoneVBO; //Zabranjena zona VBO
    unsigned int ledVAO; //Led lampice
    unsigned int ledVBO; //Led lampice

    createMainVao(&mainVAO, &mainVBO, &mainEBO);
    createCircleVao(&airCraftVAO[0], &airCraftVBO[0], x0[0], y0[0], r1); //Avion I
    createCircleVao(&airCraftVAO[1], &airCraftVBO[1], x0[1], y0[1], r2); //Avion II
    createCircleVao(&restrictedZoneVAO, &restrictedZoneVBO, x0[2], y0[2], r3); //Zabranjena zona
    createCircleVao(&ledVAO, &ledVBO, 0, 0, 0.5);
    //create2ndAircraftVao(&airCraftVAO[1]);


    //TEKSTURA
    unsigned mapTexture = loadImageToTexture("res/karta.jpg");
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);


    float batteryLevel[2] = {0.0, 0.0};
    bool isActive[2] = { false, false };
    float ledColor1[4] = { 0.3, 0.3, 0.3, 1.0 }; //vec4 koji cu slati uniformu uCol da boji boju ledovke
    float ledColor2[4] = { 0.3, 0.3, 0.3, 1.0 }; //vec4 koji cu slati uniformu uCol da boji boju ledovke
    float aircraftColor1[4] = { 0.0, 0.0, 1.0, 1.0 };
    float aircraftColor2[4] = { 0.0, 0.0, 1.0, 1.0 };
    float resizingColor[4] = { 0.1, 0.5, 0.1, 0.5 };


    const double targetFrameTime = 1.0 / 60.0;
    int test = 60;
    int i = 0;

    float dx[2] = { 0.0,0.0 };
    float dy[2] = { 0.0,0.0 };


    unsigned int displayTimer1 = 0, displayTimer2 = 0;

    unsigned int uCol = glGetUniformLocation(universalRectShader, "uCol");
    unsigned int uPos = glGetUniformLocation(universalRectShader, "uPos");


    //TextRenderer textRenderer(wWidth, wHeight);
    //if (!textRenderer.Load("fonts/arial.ttf", 48)) { // Replace with your font path
    //    std::cerr << "Failed to load font for TextRenderer." << std::endl;
    //    return -1;
    //}


    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    while (!glfwWindowShouldClose(window))
    {
        //std::cout << resizingRestrictedZone << std::endl;
        /*std::cout << batteryLevel[0] << std::endl;
        std::cout << batteryLevel[1] << std::endl;*/
        //std::cout << "x: "<< x0[0] << ", y: " << y0[0] << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

      /*  if (test == 0) {
            test = 60;
            i++;
            std::cout << i << std::endl;
        }
        else {
            test--;
        }*/

        glClear(GL_COLOR_BUFFER_BIT);
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        if (isActive[0] && batteryLevel[0] > -2.0) {
            batteryLevel[0] -= 1 / 1000.0;
        }
        if (isActive[1] && batteryLevel[1] > -2.0) {
            batteryLevel[1] -= 1 / 1000.0;
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS && aircraftColor1[3] == 1.0) {
            ledColor1[0] = 1.0f;
            ledColor1[1] = 0.3f;
            ledColor1[2] = 0.3f;
            ledColor1[3] = 1.0f;
            isActive[0] = true;
        }
        if ((glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) || batteryLevel[0] <= -2.0) {
            ledColor1[0] = 0.3f;
            ledColor1[1] = 0.3f;
            ledColor1[2] = 0.3f;
            ledColor1[3] = 1.0f;
            isActive[0] = false;
        }
        
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS && aircraftColor2[3] == 1.0) {
            ledColor2[0] = 1.0f;
            ledColor2[1] = 0.3f;
            ledColor2[2] = 0.3f;
            ledColor2[3] = 1.0f;
            isActive[1] = true;
        }
        if ((glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) || batteryLevel[1] <= -2.0) {
            ledColor2[0] = 0.3f;
            ledColor2[1] = 0.3f;
            ledColor2[2] = 0.3f;
            ledColor2[3] = 1.0f;
            isActive[1] = false;
        }

        if (aircraftColor1[3] != 0.0 && (batteryLevel[0] <= -2.0 || pointIsOutOfBounds(x0[0], y0[0]) || (circlesCollide(x0[0], y0[0], r1, x0[1], y0[1], r2) && aircraftColor2[3] != 0.0) || circlesCollide(x0[0], y0[0], r1, x0[2], y0[2], gRestrictedZoneRadius))) {
            ledColor1[0] = 0.3f;
            ledColor1[1] = 0.3f;
            ledColor1[2] = 0.3f;
            ledColor1[3] = 1.0f;
            isActive[0] = false;
            aircraftColor1[3] = 0.0;
            displayTimer1 = 240;
            if (circlesCollide(x0[0], y0[0], r1, x0[1], y0[1], r2)) { //ako je bio ovaj slucaj ostavi avion na sceni pa ih oba iskljuci u sledecem if-u, moglo se izdvojiti i u zaseban if
                aircraftColor1[3] = 1.0;
            }
            std::cout << "I true" << std::endl;
        }

        if (aircraftColor2[3] != 0.0 && (batteryLevel[1] <= -2.0 || pointIsOutOfBounds(x0[1], y0[1]) || (circlesCollide(x0[0], y0[0], r1, x0[1], y0[1], r2) && aircraftColor1[3] != 0.0) || circlesCollide(x0[2], y0[2], gRestrictedZoneRadius, x0[1], y0[1], r2))) {
            ledColor2[0] = 0.3f;
            ledColor2[1] = 0.3f;
            ledColor2[2] = 0.3f;
            ledColor2[3] = 1.0f;
            isActive[1] = false;
            aircraftColor2[3] = 0.0;
            displayTimer2 = 240;
            if (circlesCollide(x0[0], y0[0], r1, x0[1], y0[1], r2)) {
                aircraftColor1[3] = 0.0;
            }
            std::cout << "II true" << std::endl;
        }

        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && isActive[0]){
            dy[0] += SPEED;
            y0[0] += SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && isActive[0]){
            dy[0] -= SPEED;
            y0[0] -= SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && isActive[0]){
            dx[0] += SPEED;
            x0[0] += SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && isActive[0]){
            dx[0] -= SPEED;
            x0[0] -= SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && isActive[1]){
            dy[1] += SPEED;
            y0[1] += SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && isActive[1]){
            dy[1] -= SPEED;
            y0[1] -= SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && isActive[1]){
            dx[1] += SPEED;
            x0[1] += SPEED;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && isActive[1]){
            dx[1] -= SPEED;
            x0[1] -= SPEED;
        }


        //RESTART
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            batteryLevel[0] = batteryLevel[1] = 0.0;
            isActive[0] = isActive[1] = false;
            float initialLedColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
            std::copy(initialLedColor, initialLedColor + 4, ledColor1);
            std::copy(initialLedColor, initialLedColor + 4, ledColor2);
            float initialAircraftColor[4] = { 0.0, 0.0, 1.0, 1.0 };
            std::copy(initialAircraftColor, initialAircraftColor + 4, aircraftColor1);
            std::copy(initialAircraftColor, initialAircraftColor + 4, aircraftColor2);
            x0[0] = -0.7;
            x0[1] = 0.58;
            y0[0] = -0.7;
            y0[1] = -0.5;
            dx[0] = dx[1] = dy[0] = dy[1] = 0.0;
            displayTimer1 = displayTimer2 = 0;
            gRestrictedZoneRadius = 0.1;
            createCircleVao(&restrictedZoneVAO, &restrictedZoneVBO, x0[2], y0[2], gRestrictedZoneRadius);
        }

        //CRTANJE PRVE LED-ovke
        glUseProgram(universalRectShader);
        glBindVertexArray(ledVAO);
        glUniform4fv(uCol, 1, ledColor1);
        glUniform2f(uPos, 0.0, 0.0);
        glViewport(0, (wHeight - 600)/2, (wWidth - 1100) / 2, (wWidth - 1100) / 2);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ((CRES * 2 + 4)*4) / (2 * sizeof(float)));

        //CRTANJE DRUGE LED-ovke
        glUseProgram(universalRectShader);
        glBindVertexArray(ledVAO);
        glUniform4fv(uCol, 1, ledColor2);
        glUniform2f(uPos, 0.0, 0.0);
        glViewport(1100 + (wWidth - 1100) / 2, (wHeight - 600) / 2, (wWidth - 1100) / 2, (wWidth - 1100) / 2);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ((CRES * 2 + 4) * 4) / (2 * sizeof(float)));


        //CRTANJE POZADINE ZA BATERIJE
        glUseProgram(universalRectShader);
        glBindVertexArray(mainVAO);
        glUniform4f(uCol, 1.0, 1.0, 1.0, 1.0);
        glUniform2f(uPos, 0.0, 0.0);
        glViewport(0, wHeight - 600, wWidth, 600);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        //CRTANJE MAPE
        glUseProgram(mapShader);
        glBindTexture(GL_TEXTURE_2D, mapTexture);
        //glBindVertexArray(mainVAO);
        //glViewport(100, wHeight * 0.3, wWidth - 200, wHeight - wHeight * 0.31);
        //okvir (1280-1100)/2 = 90, 800-600=200, 1100, 600
        glViewport((wWidth - 1100) / 2, wHeight - 600, 1100, 600);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        //ZELENI FILTER
        glUseProgram(filterShader);
        glBindVertexArray(mainVAO);
        //glViewport(100, wHeight * 0.3, wWidth - 200, wHeight - wHeight * 0.31);
        glViewport((wWidth - 1100) / 2, wHeight - 600, 1100, 600);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        //BATERIJA I
        percentage1 = mapToPercentage(batteryLevel[0]);
        //std::cout << percentage1 << std::endl;
        glViewport(0, wHeight - 600, (wWidth - 1100) / 2, 600);
        glUseProgram(universalRectShader);
        glUniform2f(uPos, 0.0, batteryLevel[0]);
        if (mapToPercentage(batteryLevel[0]) >= 75) {
            glUniform4f(uCol, 0.2f, 1.0f, 0.2f, 1.0f); // Vibrant green
        }
        else if (mapToPercentage(batteryLevel[0]) >= 26 && mapToPercentage(batteryLevel[0]) <= 74) {
            glUniform4f(uCol, 1.0f, 0.8f, 0.2f, 1.0f); // Golden yellow
        }
        else {
            glUniform4f(uCol, 0.8f, 0.0f, 0.0f, 1.0f); // Darker red
        }
        glBindVertexArray(mainVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        //NIVO BATERIJE I
        glViewport(0, 0, wWidth, wHeight);
        RenderText(std::to_string(mapToPercentage(batteryLevel[0])) + "%", 3.0f, wHeight - 300, 0.7f, glm::vec3(0.0, 0.0, 0.0), textShader, VAO, VBO);
        
        //BATERIJA II
        percentage2 = mapToPercentage(batteryLevel[1]);
        //std::cout << percentage2 << std::endl;
        glViewport((wWidth - 1100) / 2  + 1100, wHeight - 600, (wWidth - 1100) / 2, 600);
        glUseProgram(universalRectShader);
        glUniform2f(uPos, 0.0, batteryLevel[1]);
        if (mapToPercentage(batteryLevel[1]) >= 75) {
            glUniform4f(uCol, 0.2f, 1.0f, 0.2f, 1.0f); // Vibrant green
        }
        else if (mapToPercentage(batteryLevel[1]) >= 26 && mapToPercentage(batteryLevel[1]) <= 74) {
            glUniform4f(uCol, 1.0f, 0.8f, 0.2f, 1.0f); // Golden yellow
        }
        else {
            glUniform4f(uCol, 0.8f, 0.0f, 0.0f, 1.0f); // Darker red
        }
        glBindVertexArray(mainVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        //NIVO BATERIJE I
        glViewport(0, 0, wWidth, wHeight);
        RenderText(std::to_string(mapToPercentage(batteryLevel[1])) + "%", wWidth- ((wWidth - 1100) / 2)+3, wHeight - 300, 0.7f, glm::vec3(0.0, 0.0, 0.0), textShader, VAO, VBO);

        //INTERAKTIVNI OBJEKTI
        //AVION I
        glViewport((wWidth - 1100) / 2, wHeight - 600, 1100, 1100);
        glUseProgram(universalRectShader);
        glBindVertexArray(airCraftVAO[0]);
        glUniform2f(uPos, dx[0], dy[0]);
        glUniform4fv(uCol, 1, aircraftColor1);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ((CRES * 2 + 4) * 4) / (2 * sizeof(float)));


        //AVION II
        glUseProgram(universalRectShader);
        glBindVertexArray(airCraftVAO[1]);
        glUniform2f(uPos, dx[1], dy[1]);
        glUniform4fv(uCol, 1, aircraftColor2);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ((CRES * 2 + 4) * 4) / (2 * sizeof(float)));

        //ZABRANJENA ZONA
        if (resizingRestrictedZone) {
            gRestrictedZoneRadius += 0.001f;  // Increment radius
            std::copy(resizingColor, resizingColor + 4, restrictedZoneColor);  // Change color

            // Update VBO data with new radius
            float vertices[CRES * 2 + 4];
            vertices[0] = x0[2];
            vertices[1] = y0[2];


            glDeleteVertexArrays(1, &restrictedZoneVAO);
            glDeleteBuffers(1, &restrictedZoneVBO);

            createCircleVao(&restrictedZoneVAO, &restrictedZoneVBO, x0[2], y0[2], gRestrictedZoneRadius);
        }
        glUseProgram(universalRectShader);
        glBindVertexArray(restrictedZoneVAO);
        glUniform2f(uPos, 0, 0);
        glUniform4fv(uCol, 1, restrictedZoneColor);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ((CRES * 2 + 4) * 4) / (2 * sizeof(float)));

        //ISPISI UNISTENJA LETELICA
        if (displayTimer1 != 0) {
            glViewport(0, 0, wWidth, wHeight);
            RenderText("Letelica br. I je unistena!", (wWidth - 1100) / 2, (wHeight - 600) / 2, 0.7f, glm::vec3(1.0, 0.0, 0.0), textShader, VAO, VBO);
            displayTimer1--;
        }
        if (displayTimer2 != 0) {
            glViewport(0, 0, wWidth, wHeight);
            RenderText("Letelica br. II je unistena!", wWidth - 450, (wHeight - 600) / 2, 0.7f, glm::vec3(1.0, 0.0, 0.0), textShader, VAO, VBO);
            displayTimer2--;
        }

        //KOORDINATE
        glm::vec2 pixelCoords1 = normalizeToPixels(x0[0], y0[0], 1100, 600);
        glm::vec2 pixelCoords2 = normalizeToPixels(x0[1], y0[1], 1100, 600);
        int x1 = static_cast<int>(pixelCoords1.x);
        int y1 = static_cast<int>(pixelCoords1.y);

        int x2 = static_cast<int>(pixelCoords2.x);
        int y2 = static_cast<int>(pixelCoords2.y);
        glViewport(0, 0, wWidth, wHeight);
        RenderText("(" + std::to_string(x1) + ", " + std::to_string(y1) + ")", (wWidth - 1100) / 2, wHeight-650, 0.7f, glm::vec3(1.0, 1.0, 1.0), textShader, VAO, VBO);
        RenderText("(" + std::to_string(x2) + ", " + std::to_string(y2) + ")", wWidth - ((wWidth - 800) / 2), wHeight - 650, 0.7f, glm::vec3(1.0, 1.0, 1.0), textShader, VAO, VBO);
        //RenderText(std::to_string, 0.0f, 10.0f, 0.7f, glm::vec3(1.0, 1.0, 0.0), textShader, VAO, VBO);
        //std::cout << "X Pixels: " << pixelCoords.x << ", Y Pixels: " << pixelCoords.y << std::endl;
        
        //textRenderer.RenderText("test", 100.0f, 100.0f, 0.7f, glm::vec3(1.0, 0.0, 0.0));
        //INDEX
        glViewport(0, 0, wWidth, wHeight);
        RenderText("Ognjen Milicevic RA149/2020", 0.0f, 10.0f, 0.7f, glm::vec3(1.0, 1.0, 0.0), textShader, VAO, VBO);

        glBindVertexArray(0);
        glUseProgram(0);

        glfwSwapBuffers(window);
        glfwPollEvents();

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedTime = endTime - startTime;
        double frameDuration = elapsedTime.count();
        double sleepTime = targetFrameTime - frameDuration;

        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
    }

    glDeleteTextures(1, &mapTexture);

    glDeleteVertexArrays(1, &mainVAO);
    glDeleteVertexArrays(1, &airCraftVAO[0]);
    glDeleteVertexArrays(1, &airCraftVAO[1]);
    glDeleteVertexArrays(1, &restrictedZoneVAO);
    glDeleteVertexArrays(1, &ledVAO);
    glDeleteBuffers(1, &mainVBO);
    glDeleteBuffers(1, &airCraftVBO[0]);
    glDeleteBuffers(1, &airCraftVBO[1]);
    glDeleteBuffers(1, &restrictedZoneVBO);
    glDeleteBuffers(1, &ledVBO);
    glDeleteBuffers(1, &mainEBO);
    glDeleteProgram(mapShader);
    glDeleteProgram(filterShader);
    glDeleteProgram(universalRectShader);
    glDeleteProgram(textShader);
    //glDeleteTextures(1, &mapTexture);
    //glDeleteBuffers(1, &VBO);
    //glDeleteVertexArrays(1, &VAO);
    //glDeleteProgram(unifiedShader);

    glfwTerminate();
    return 0;
}

glm::vec2 normalizeToPixels(float x, float y, int screenWidth = 1100, int screenHeight = 600) {
    float xPixels = ((x + 1.0f) / 2.0f) * screenWidth;
    float yPixels = ((y + 1.0f) / 1.1f) * screenHeight;
    return glm::vec2(xPixels, yPixels);
}

int mapToPercentage(float translationFactor) {
    return static_cast<int>(100 + (translationFactor / 2.0f) * 100);
}

// Function to check if two circles (or spheres in 2D) are colliding.
bool circlesCollide(float cx1, float cy1, float r1, float cx2, float cy2, float r2) {
    // Calculate the distance between the centers of the two circles using the distance formula.
    float distBetweenCenters = std::sqrt(std::pow(cx2 - cx1, 2) + std::pow(cy2 - cy1, 2));

    // Return true if the distance is less than the sum of the radii, meaning the circles overlap.
    return (distBetweenCenters < r1 + r2);
}

// Function to check if a point is outside a defined boundary.
bool pointIsOutOfBounds(float px, float py) {
    // Return true if the point's x or y coordinates fall outside the specified range.
    // The range for x is [-1.0, 1.0], and for y is [-1.0, -0.08].
    return px < -1.0f || px > 1.0f || py < -1.0f || py > 0.1;//-0.09f;
}

void createMainVao(unsigned int* VAO, unsigned int* VBO, unsigned int* EBO)
{
    float vertices[] =
    {
         1.0, 1.0,      1.0, 1.0,
         1.0, -1.0,     1.0, 0.2,
        -1.0, -1.0,     0.0, 0.2,
        -1.0, 1.0,      0.0, 1.0,
    };

    unsigned int indices[] = {
        0, 1, 3,   
        1, 2, 3    
    };

    //unsigned int VBO, EBO;

    glGenVertexArrays(1, VAO);

    glGenBuffers(1, VBO);
    glGenBuffers(1, EBO);
    
    glBindVertexArray(*VAO);

    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);


}

void createCircleVao(unsigned int* VAO, unsigned int* VBO, float x0, float y0, float r) {

    float vertices[CRES * 2 + 4]; //Aircraft I vertices
    //float r = 0.02;

    //float center[2] = { -0.7, -0.7 };

    vertices[0] = x0;
    vertices[1] = y0;

    for (int i = 0; i <= CRES; i++)
    {
        vertices[2 + 2 * i] = r * cos((3.141592 / 180) * (i * 360 / CRES)) + x0;
        vertices[2 + 2 * i + 1] = r * sin((3.141592 / 180) * (i * 360 / CRES)) + y0;
    }

    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);

    glBindVertexArray(*VAO);
    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

//void create2ndAircraftVao(unsigned int* VAO, float x0, float y0, float r) {
//    unsigned int VBO;
//    float vertices[CRES * 2 + 4]; //Aircraft I vertices
//    //float r = 0.02;
//
//    //float center[2] = { 0.58, -0.5 };
//
//    for (int i = 0; i <= CRES; i++)
//    {
//        vertices[2 + 2 * i] = r * cos((3.141592 / 180) * (i * 360 / CRES)) + x[0];
//        vertices[2 + 2 * i + 1] = r * sin((3.141592 / 180) * (i * 360 / CRES)) + y[0];
//    }
//
//    glGenVertexArrays(1, VAO);
//    glGenBuffers(1, &VBO);
//
//    glBindVertexArray(*VAO);
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//
//    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
//    glEnableVertexAttribArray(0);
//
//    glBindVertexArray(0);
//    glBindBuffer(GL_ARRAY_BUFFER, 0);
//}

unsigned int compileShader(GLenum type, const char* source)
{
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }
     std::string temp = ss.str();
     const char* sourceCode = temp.c_str();

    int shader = glCreateShader(type);
    
    int success;
    char infoLog[512];
    glShaderSource(shader, 1, &sourceCode, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}
unsigned int createShader(const char* vsSource, const char* fsSource)
{
    
    unsigned int program;
    unsigned int vertexShader;
    unsigned int fragmentShader;

    program = glCreateProgram();

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource);
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource);

    
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    glValidateProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "Objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}
static unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);
    if (ImageData != NULL)
    {
        //Slike se osnovno ucitavaju naopako pa se moraju ispraviti da budu uspravne
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        // Provjerava koji je format boja ucitane slike
        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        // oslobadjanje memorije zauzete sa stbi_load posto vise nije potrebna
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        std::cout << "Textura nije ucitana! Putanja texture: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return 0;
    }
}

void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color, unsigned int shaderId, unsigned int VAO, unsigned int VBO) {
    // Save previous OpenGL states
    GLboolean blendingEnabled;
    glGetBooleanv(GL_BLEND, &blendingEnabled);
    GLint currentBlendSrc, currentBlendDst;
    glGetIntegerv(GL_BLEND_SRC, &currentBlendSrc);
    glGetIntegerv(GL_BLEND_DST, &currentBlendDst);
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    GLint currentVAO;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);

    // Enable blending for text rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Activate corresponding render state	
    glUseProgram(shaderId);
    glUniform3f(glGetUniformLocation(shaderId, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursors for next glyph
        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (1/64th units)
    }

    // Restore previous OpenGL states
    if (!blendingEnabled) {
        glDisable(GL_BLEND);
    }
    glBlendFunc(currentBlendSrc, currentBlendDst);
    glUseProgram(currentProgram);
    glBindVertexArray(currentVAO);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    float initialRestrictedZoneColor[4] = { 0.5, 0.1, 0.1, 0.5 };
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        //std::cout << "EVENT DETECTED" << std::endl;
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);


        // Get viewport position and size
        int viewportX = (1280 - 1100) / 2;
        int viewportY = 800 - 600;
        int viewportWidth = 1100;
        int viewportHeight = 1100;
        //std::cout << mouseY << std::endl;
        if (mouseX >= viewportX && mouseX <= viewportX + viewportWidth &&
            mouseY >= 0 && mouseY <= 600) {
        
            mouseX -= viewportX;
            mouseY -= viewportY;
            float xNorm = (mouseX / viewportWidth) * 2.0f - 1.0f;
            float yNorm = (1.0f - (mouseY / viewportHeight) * 2.0f)-1.272;
            //float yNorm = 1.0f - (mouseY / viewportHeight) * 2.0f;
            //std::cout << "Normalized coords: " << xNorm << ", " << yNorm << std::endl;
            // Check if the click is inside the restricted zone
            float dx = xNorm - gx0;  // gx0 is the zone center x-coordinate (normalized) current 0.03
            float dy = yNorm - gy0;  // gy0 is the zone center y-coordinate (normalized) current -0.80
            float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= gRestrictedZoneRadius) {  // gRestrictedZoneRadius is in normalized space
                std::cout << "Inside restricted zone" << std::endl;
                resizingRestrictedZone = true;
            }
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        resizingRestrictedZone = false;  // Stop resizing
        std::copy(initialRestrictedZoneColor, initialRestrictedZoneColor + 4, restrictedZoneColor);
    }
}