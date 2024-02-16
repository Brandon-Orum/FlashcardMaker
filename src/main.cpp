#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <windows.h>
#include <wingdi.h>

#include <json.h>


//////////////put into seperate library
#include <algorithm> 
#include <cctype>
#include <locale>

// trim from start (in place)
inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string& s) {
    rtrim(s);
    ltrim(s);
}

void toLowercase(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return std::tolower(c); });
}

std::vector<std::string> splitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> strings;

    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos)
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.size();
    }

    // To get the last substring (or only, if delimiter is not found)
    strings.push_back(str.substr(prev));

    return strings;
}
////////////

void copyFromClipboard(cv::Mat& mat) {
    HBITMAP hbitmap = nullptr;
    if (!::OpenClipboard(nullptr))
        return;
    if (IsClipboardFormatAvailable(CF_BITMAP))
        hbitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!hbitmap && IsClipboardFormatAvailable(CF_DIB))
    {
        HANDLE handle = GetClipboardData(CF_DIB);
        LPVOID hmem = GlobalLock(handle);
        if (hmem)
        {
            BITMAPINFO* bmpinfo = (BITMAPINFO*)hmem;
            int offset = (bmpinfo->bmiHeader.biBitCount > 8) ?
                0 : sizeof(RGBQUAD) * (1 << bmpinfo->bmiHeader.biBitCount);
            BYTE* bits = (BYTE*)(bmpinfo)+bmpinfo->bmiHeader.biSize + offset;
            HDC hdc = ::GetDC(0);
            hbitmap = CreateDIBitmap(hdc,
                &bmpinfo->bmiHeader, CBM_INIT, bits, bmpinfo, DIB_RGB_COLORS);
            ::ReleaseDC(0, hdc);
            GlobalUnlock(hmem);
        }
    }

    if (hbitmap)
    {
        BITMAP bm;
        ::GetObject(hbitmap, sizeof(bm), &bm);
        int cx = bm.bmWidth;
        int cy = bm.bmHeight;
        if (bm.bmBitsPixel == 32)
        {
            mat.create(cy, cx, CV_8UC4);
            BITMAPINFOHEADER bi = { sizeof(bi), cx, -cy, 1, 32, BI_RGB };
            HDC hdc = ::GetDC(0);
            GetDIBits(hdc, hbitmap, 0, cy, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        }
    }
    CloseClipboard();
}


void multiLinePutText(cv::Mat img,
    std::string text,
    cv::Point pos,
    int fontFace = cv::HersheyFonts::FONT_HERSHEY_SIMPLEX,
    float fontScale = 0.5,
    cv::Scalar color = cv::Scalar(0, 0, 0, 255),
    int thickness = 1,    
    float lineSpacing = 1.0)
{
    std::vector<std::string> lines = splitString(text, "\n");
    for (std::string line : lines) {
        int baseLine;
        cv::Size textSize = cv::getTextSize(line, fontFace, fontScale, thickness, &baseLine);
        cv::putText(img, line, pos, fontFace, fontScale, color, thickness);
        pos += cv::Point(0, textSize.height * lineSpacing);
    }
}

//make a file name for saving the flashcard
void makeFileName(char* fileName) {    
    time_t t = time(0);
    struct tm* now = localtime(&t);
    strftime(fileName, 128, "Flashcard-%Y-%m-%d-%H-%M-%S", now);
}

std::vector<std::string> getAllTopics() {
    std::vector<std::string> listOfTopics;
    return listOfTopics;
}

std::vector<std::string> searchForFlashcards(std::string directory, std::string topic, std::vector<std::string> keywords) {
    std::vector<std::string> flashcardFilenames;
    // check if directory/topic is a valid directory
    // open each json file found in the directory
    // itterate through the keywords found in the json file
    // if all the keywords listed in keywords are found, then add the name of the file to flashcardFilenames
    struct stat sb;
    std::string flashcardDirectoryStr = std::string(directory + "/" + topic);
    const char* flashcardDirectory = flashcardDirectoryStr.c_str();
    if (stat(flashcardDirectory, &sb) == 0) {
        for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(flashcardDirectory)) {
            if (dirEntry.path().extension() == ".json") {
                Json::Value root;
                std::ifstream ifs;
                ifs.open(dirEntry);

                Json::CharReaderBuilder builder;
                builder["collectComments"] = true;
                JSONCPP_STRING errs;
                if (!parseFromStream(builder, ifs, &root, &errs)) {
                    return flashcardFilenames;
                    std::cerr << errs << std::endl;
                }

                std::cout << root << std::endl;

                std::vector<std::string> flashcardKeywords;
                if (root.isMember("keywords")) {
                    for (auto flashcardKeyword : root["keywords"]) {
                        flashcardKeywords.push_back(flashcardKeyword.asString());
                    }
                }

                //check if all the keywords being searched for are found in this flashcard
                bool allKeywordsFound = true;
                for (auto keyword : keywords) {
                    if (keyword.empty()) continue;
                    if (std::find(flashcardKeywords.begin(), flashcardKeywords.end(), keyword) == flashcardKeywords.end()) {
                        allKeywordsFound = false;
                    }
                }
                if (!allKeywordsFound) {
                    continue;
                }

                flashcardFilenames.push_back(dirEntry.path().filename().stem().string());

                std::vector<std::pair<cv::Point, cv::Point>> answerBoxPositions;
                if (root.isMember("answerBoxPositionsList")) {
                    for (auto boxBounds : root["answerBoxPositionsList"]) {
                        cv::Point boxPosition(boxBounds[0][0].asInt(), boxBounds[0][1].asInt());
                        cv::Point boxEndPosition(boxBounds[1][0].asInt(), boxBounds[1][1].asInt());
                        std::pair<cv::Point, cv::Point> boxBounds(boxPosition, boxEndPosition);
                        answerBoxPositions.push_back(boxBounds);
                    }
                }

                std::vector<std::pair<cv::Point, cv::Point>> questionBoxPositions;
                if (root.isMember("questionBoxPositionsList")) {
                    for (auto boxBounds : root["answerBoxPositionsList"]) {
                        cv::Point boxPosition(boxBounds[0][0].asInt(), boxBounds[0][1].asInt());
                        cv::Point boxEndPosition(boxBounds[1][0].asInt(), boxBounds[1][1].asInt());
                        std::pair<cv::Point, cv::Point> boxBounds(boxPosition, boxEndPosition);
                        questionBoxPositions.push_back(boxBounds);
                    }
                }
            }            
        }
    }
    else {
        std::cerr << "Error searching for flashcards:" << std::endl;
        std::cerr << "Folder with name: " << topic << " not found." << std::endl;
    }
    
    return flashcardFilenames;
}

cv::Mat loadFlashcardImage(std::string flashcardSavePath, std::string topic, std::string fileName) {
    //cv imread needs an absolute path to read the image
    std::filesystem::path fnPath(flashcardSavePath + "/" + "test topic" + "/" + fileName + ".png");
    std::string fnPathStr = std::filesystem::absolute(fnPath).string();
    cv::Mat img = cv::imread(fnPathStr);
    cv::cvtColor(img, img, cv::COLOR_BGR2RGBA);
    return img;
}

void loadFlashcardBoxBounds(std::string flashcardSavePath, std::string topic, std::string fileName,
    std::vector<std::pair<cv::Point, cv::Point>> &answerBoxBounds,
    std::vector<std::pair<cv::Point, cv::Point>> &questionBoxBounds) {

    Json::Value root;
    std::ifstream ifs;
    ifs.open(flashcardSavePath + "/" + topic + "/" + fileName + ".json");

    Json::CharReaderBuilder builder;
    builder["collectComments"] = true;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cerr << errs << std::endl;
        return;        
    }

    if (root.isMember("answerBoxPositionsList")) {
        for (auto boxBounds : root["answerBoxPositionsList"]) {
            cv::Point boxPosition(boxBounds[0][0].asInt(), boxBounds[0][1].asInt());
            cv::Point boxEndPosition(boxBounds[1][0].asInt(), boxBounds[1][1].asInt());
            std::pair<cv::Point, cv::Point> boxBounds(boxPosition, boxEndPosition);
            answerBoxBounds.push_back(boxBounds);
        }
    }

    if (root.isMember("questionBoxPositionsList")) {
        for (auto boxBounds : root["answerBoxPositionsList"]) {
            cv::Point boxPosition(boxBounds[0][0].asInt(), boxBounds[0][1].asInt());
            cv::Point boxEndPosition(boxBounds[1][0].asInt(), boxBounds[1][1].asInt());
            std::pair<cv::Point, cv::Point> boxBounds(boxPosition, boxEndPosition);
            questionBoxBounds.push_back(boxBounds);
        }
    }
}

bool topicsFilterCallbackCalled = false;
int topicsFilterCallback(ImGuiInputTextCallbackData* data) {
    topicsFilterCallbackCalled = true;
    return 0;
}

bool keywordsFilterCallbackCalled = false;
int keywordsFilterCallback(ImGuiInputTextCallbackData* data) {
    keywordsFilterCallbackCalled = true;
    return 0;
}

int main( int argc, char* argv[] ) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LogLevel::LOG_LEVEL_SILENT);

    //read config file
    Json::Value configRoot;
    std::ifstream ifs;
    ifs.open("../defaultConfig.json");

    Json::CharReaderBuilder builder;
    builder["collectComments"] = true;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &configRoot, &errs)) {
        std::cout << "Error reading config file" << std::endl;
        std::cout << errs << std::endl;
        return -1;
    }
    
    char fileName[128];
    makeFileName(fileName);

    cv::Mat image;
    cv::Mat imageFromClipboard;
    copyFromClipboard(imageFromClipboard);
    if (imageFromClipboard.empty()) {
        //if no image is found in the clipboard, display a blank flashcard
        image = cv::Mat(400, 800, CV_8UC4, cv::Scalar(255, 255, 255, 255));
    }
    else {
        //if an image is found in the clipboard, increase the canvas size around the image
        cv::Size cs = imageFromClipboard.size() + cv::Size(200, 200);
        image = cv::Mat(cs.height, cs.width, CV_8UC4, cv::Scalar(255, 255, 255, 255));
        imageFromClipboard.copyTo(image(cv::Rect(100, 100, imageFromClipboard.cols, imageFromClipboard.rows)));
    }
    cv::cvtColor(image, image, cv::COLOR_BGR2RGBA);
    cv::Mat canvasMat;
    cv::Mat currentFlashcardImage = cv::Mat(400, 800, CV_8UC4, cv::Scalar(255, 255, 255, 255));
    cv::Mat currentFlashcardCanvas;

    //buffers
    static char textBuffer[1000*1000] = "";
    static char topicBuffer[128] = "";
    static char keywordsBuffer[1000] = "";

    if( !glfwInit() ){
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow( 1000, 600, "Flashcards", nullptr, nullptr );
    glfwSetWindowCloseCallback( window, []( GLFWwindow* window ){ glfwSetWindowShouldClose( window, GL_FALSE ); } );
    glfwMakeContextCurrent( window );
    glfwSwapInterval( 1 );
    gl3wInit();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOpenGL( window, true );
    ImGui_ImplOpenGL3_Init( "#version 330" );

    //apply ImGui configurations
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    int font = cv::FONT_HERSHEY_COMPLEX;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    bool is_show = true;
    while( is_show ){
        glfwPollEvents();
        glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Create flashcard", &is_show);

        image.copyTo(canvasMat);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 canvasOffset(0.0f, 0.0f);

        static int addMode = 0;

        static bool textPlaced = false;
        static bool textBoxFocused = false;
        static bool imagePlaced = false;
        static int imagePlacementDirectionX = 1;
        static int imagePlacementDirectionY = 1;        
        static cv::Point2f textPosition;

        static cv::Point boxPosition(0, 0);
        static std::vector<std::pair<cv::Point, cv::Point>> answerBoxPositions;
        static std::vector<std::pair<cv::Point, cv::Point>> questionBoxPositions;

        int imageShiftAmountX = 0;
        int imageShiftAmountY = 0;

        ImGuiIO io = ImGui::GetIO();

        if (addMode == 0 && !textPlaced && !imagePlaced) {
            if ((mousePos.x - pos.x - canvasOffset.x > 0 && mousePos.x - pos.x - canvasOffset.x < canvasMat.cols) &&
                (mousePos.y - pos.y - canvasOffset.y > 0 && mousePos.y - pos.y - canvasOffset.y < canvasMat.rows)) {

                cv::String str = "Add text or image here\nYou may paste";

                
                //for (int i = 0; i < 512; i++) {
                //    if (io.KeysDown[i]) {
                //        int t = 1;
                //    }
                //}                

                //left arrow
                if (io.KeysDown[263]) imagePlacementDirectionX = -1;
                //right arrow
                else if (io.KeysDown[262]) imagePlacementDirectionX = 1;

                //up arrow
                if (io.KeysDown[265]) imagePlacementDirectionY = -1;
                //down arrow
                else if (io.KeysDown[264]) imagePlacementDirectionY = 1;

                //check if ctrl+v is being pressed
                static bool ctrlVPressed = false;
                if (io.KeysDown[341] && io.KeysDown[86]) ctrlVPressed = true;

                if (ctrlVPressed) {
                    copyFromClipboard(imageFromClipboard);
                    if (imageFromClipboard.empty()) {
                        const char* ts = ImGui::GetClipboardText();
                        if (ts != nullptr) {
                            str = std::string(ts);
                            std::strcpy(textBuffer, str.c_str());
                        }
                    }
                    else {
                        cv::Point topLeftImagePos(mousePos.x - pos.x, mousePos.y - pos.y);
                        cv::Point bottomRightImagePos = topLeftImagePos + cv::Point(imageFromClipboard.cols, imageFromClipboard.rows);
                        if (imagePlacementDirectionX == -1) {
                            topLeftImagePos.x -= imageFromClipboard.cols;
                            bottomRightImagePos.x -= imageFromClipboard.cols;
                        }
                        if (imagePlacementDirectionY == -1) {
                            topLeftImagePos.y -= imageFromClipboard.rows;
                            bottomRightImagePos.y -= imageFromClipboard.rows;
                        }

                        cv::Point topLeftCutoffPoint(0, 0);
                        cv::Point bottomRightCutoffPoint(imageFromClipboard.cols, imageFromClipboard.rows);

                        if (bottomRightImagePos.x > canvasMat.cols) {
                            bottomRightCutoffPoint.x -= bottomRightImagePos.x - canvasMat.cols;
                            bottomRightImagePos.x = canvasMat.cols;
                        }
                        if (bottomRightImagePos.y > canvasMat.rows) {
                            bottomRightCutoffPoint.y -= bottomRightImagePos.y - canvasMat.rows;
                            bottomRightImagePos.y = canvasMat.rows;
                        }
                        if (topLeftImagePos.x < 0) {
                            topLeftCutoffPoint.x = -topLeftImagePos.x;
                            topLeftImagePos.x = 0;
                        }
                        if (topLeftImagePos.y < 0) {
                            topLeftCutoffPoint.y = -topLeftImagePos.y;
                            topLeftImagePos.y = 0;
                        }
                        int pasteImageRecLimitY = imageFromClipboard.rows;
                        if (pasteImageRecLimitY + mousePos.y - pos.y > canvasMat.rows) {
                            pasteImageRecLimitY = canvasMat.rows - (mousePos.y - pos.y);
                        }
                        cv::Rect visibleImagePortionRec(topLeftCutoffPoint, bottomRightCutoffPoint);
                        cv::Mat visibleImagePortion = imageFromClipboard(visibleImagePortionRec);
                        cv::Rect pasteImageRec(topLeftImagePos, bottomRightImagePos);
                        visibleImagePortion.copyTo(canvasMat(pasteImageRec));

                        if (ImGui::IsMouseDown(0)) {
                            imagePlaced = true;
                            ctrlVPressed = false;
                        }
                    }
                } else if (textBuffer[0] == 0) {
                    multiLinePutText(canvasMat, str, cv::Point(mousePos.x - pos.x, mousePos.y - pos.y), font, 0.5, cv::Scalar(0, 0, 255, 255));
                }
                else {
                    multiLinePutText(canvasMat, textBuffer, cv::Point(mousePos.x - pos.x, mousePos.y - pos.y), font, 0.5, cv::Scalar(0, 0, 255, 255));
                }
                
                if (ImGui::IsMouseClicked(0)) {
                    if (!imagePlaced) {
                        textPlaced = true;
                        textBoxFocused = false;
                        textPosition = cv::Point(mousePos.x - pos.x, mousePos.y - pos.y);
                    }
                }
            }
        }
        else if (addMode == 0 && textPlaced) {
            ImGui::Text("Text:"); ImGui::SameLine();

            static int focusOnTextboxN = 0;
            if (!textBoxFocused) {
                ImGui::SetKeyboardFocusHere();
                ImGui::InputTextMultiline("Text", textBuffer, IM_ARRAYSIZE(textBuffer), ImVec2(400, ImGui::GetTextLineHeight() * 3));
                focusOnTextboxN++;
                if (focusOnTextboxN > 20) {
                    textBoxFocused = true;
                    focusOnTextboxN = 0;
                }
            }
            else {
                ImGui::InputTextMultiline("Text", textBuffer, IM_ARRAYSIZE(textBuffer), ImVec2(400, ImGui::GetTextLineHeight() * 3));
            }
            bool applyTextButton = ImGui::Button("Apply Text");
            if (applyTextButton) {
                multiLinePutText(image, cv::String(textBuffer), textPosition, font, 0.5, cv::Scalar(0, 0, 255, 255));
                textPlaced = false;
                textBoxFocused = false;
                textBuffer[0] = 0;
            }
            
            if (textBuffer[0] == 0 && !applyTextButton) {
                cv::putText(canvasMat, "Insert text here", textPosition, font, 0.5, cv::Scalar(0, 0, 255, 255));
            }
            else {
                multiLinePutText(canvasMat, cv::String(textBuffer), textPosition, font, 0.5, cv::Scalar(0, 0, 255, 255));
            }
        }
        else if (addMode == 0 && imagePlaced) {
            imagePlaced = false;
            cv::Point topLeftImagePos(mousePos.x - pos.x, mousePos.y - pos.y);
            cv::Point bottomRightImagePos = topLeftImagePos + cv::Point(imageFromClipboard.cols, imageFromClipboard.rows);
            if (imagePlacementDirectionX == -1) {
                topLeftImagePos.x -= imageFromClipboard.cols;
                bottomRightImagePos.x -= imageFromClipboard.cols;
            }
            if (imagePlacementDirectionY == -1) {
                topLeftImagePos.y -= imageFromClipboard.rows;
                bottomRightImagePos.y -= imageFromClipboard.rows;
            }

            bool canvasResizedRight = true;
            bool canvasResizedDown = true;
            int canvasResizeAmountX = 0;
            int canvasResizeAmountY = 0;
            if (bottomRightImagePos.x > canvasMat.cols) {
                canvasResizeAmountX = bottomRightImagePos.x - canvasMat.cols;
                canvasResizedRight = true;
            }
            if (bottomRightImagePos.y > canvasMat.rows) {
                canvasResizeAmountY = bottomRightImagePos.y - canvasMat.rows;
                canvasResizedDown = true;
            }
            if (topLeftImagePos.x < 0) {
                canvasResizeAmountX = -topLeftImagePos.x;
                topLeftImagePos.x = 0;
                canvasResizedRight = false;
            }
            if (topLeftImagePos.y < 0) {
                canvasResizeAmountY = -topLeftImagePos.y;
                topLeftImagePos.y = 0;
                canvasResizedDown = false;
            }

            cv::Mat resizedCanvas(image.rows + canvasResizeAmountY, image.cols + canvasResizeAmountX, CV_8UC4, cv::Scalar(255, 255, 255, 255));
            if (!canvasResizedRight && canvasResizedDown) {
                imageShiftAmountX = canvasResizeAmountX;
            }
            else if (canvasResizedRight && !canvasResizedDown) {
                imageShiftAmountY = canvasResizeAmountY;
            }
            else if (!canvasResizedRight && !canvasResizedDown) {
                imageShiftAmountX = canvasResizeAmountX;
                imageShiftAmountY = canvasResizeAmountY;
            }
            image.copyTo(resizedCanvas(cv::Rect(imageShiftAmountX, imageShiftAmountY, image.cols, image.rows)));
            imageFromClipboard.copyTo(resizedCanvas(cv::Rect(topLeftImagePos.x, topLeftImagePos.y, imageFromClipboard.cols, imageFromClipboard.rows)));
            image = resizedCanvas;
        }
        else if (textPlaced) {
            textPlaced = false;
        }
        else if (imagePlaced) {
            imagePlaced = false;
        }
        else if (addMode == 1 || addMode == 2) {
            if ((mousePos.x - pos.x - canvasOffset.x > 0 && mousePos.x - pos.x - canvasOffset.x < canvasMat.cols) &&
                (mousePos.y - pos.y - canvasOffset.y > 0 && mousePos.y - pos.y - canvasOffset.y < canvasMat.rows)) {

                if (ImGui::IsMouseDown(0)) {
                    if (boxPosition == cv::Point(0, 0)) {
                        boxPosition = cv::Point(mousePos.x - pos.x - canvasOffset.x, mousePos.y - pos.y - canvasOffset.y);
                    }
                    else {
                        cv::Point boxEndPosition(mousePos.x - pos.x - canvasOffset.x, mousePos.y - pos.y - canvasOffset.y);
                        cv::rectangle(canvasMat, boxPosition, boxEndPosition, cv::Scalar(100 * addMode, 0, 0, 255), 2);
                    }
                }
                else {
                    if (boxPosition != cv::Point(0, 0)) {
                        cv::Point boxEndPosition(mousePos.x - pos.x - canvasOffset.x, mousePos.y - pos.y - canvasOffset.y);

                        std::pair<cv::Point, cv::Point> boxBounds(boxPosition, boxEndPosition);
                        if (addMode == 1) {
                            answerBoxPositions.push_back(boxBounds);
                        }
                        else {
                            questionBoxPositions.push_back(boxBounds);
                        }
                    }
                    boxPosition = cv::Point(0, 0);
                    cv::String str = " Click and drag to place a\n box over the answer";
                    if (addMode == 2) {
                        str = " Click and drag to place a\n box over the question";
                    }
                    multiLinePutText(canvasMat, str, cv::Point(mousePos.x - pos.x, mousePos.y - pos.y), font, 0.5, cv::Scalar(0, 0, 255, 255));
                }                
            }
            else if (!ImGui::IsMouseDown(0)) {
                boxPosition = cv::Point(0, 0);
            }
        }

        //update box positions and draw
        for (std::pair<cv::Point, cv::Point> &boxBounds : answerBoxPositions) {
            boxBounds.first += cv::Point(imageShiftAmountX, imageShiftAmountY);
            boxBounds.second += cv::Point(imageShiftAmountX, imageShiftAmountY);
            cv::rectangle(canvasMat, boxBounds.first, boxBounds.second, cv::Scalar(100, 0, 0, 255), 2);
        }
        for (std::pair<cv::Point, cv::Point> &boxBounds : questionBoxPositions) {
            boxBounds.first += cv::Point(imageShiftAmountX, imageShiftAmountY);
            boxBounds.second += cv::Point(imageShiftAmountX, imageShiftAmountY);
            cv::rectangle(canvasMat, boxBounds.first, boxBounds.second, cv::Scalar(200, 0, 0, 255), 2);
        }

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, canvasMat.cols, canvasMat.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvasMat.data );
        ImGui::Image( reinterpret_cast<void*>( static_cast<intptr_t>( texture ) ), ImVec2(canvasMat.cols, canvasMat.rows ) );

        bool addTextButton = ImGui::RadioButton("Add Text/Image", &addMode, 0); ImGui::SameLine();
        bool addAnswerBoxButton = ImGui::RadioButton("Add Answer Box", &addMode ,1); ImGui::SameLine();
        bool addQuestionBoxButton = ImGui::RadioButton("Add Question Box", &addMode, 2);

        ImGui::Text("Topic:"); ImGui::SameLine();
        ImGui::InputText("", topicBuffer, IM_ARRAYSIZE(topicBuffer));

        ImGui::Text("Keywords (separate with commas):");
        ImGui::InputTextWithHint("Keywords", "ie. dog breed, country, ect.", keywordsBuffer, IM_ARRAYSIZE(keywordsBuffer));

        bool saveButton = ImGui::Button("Save Flashcard"); ImGui::SameLine();
        if (saveButton && topicBuffer[0] != 0) {
            std::string filePath = configRoot["flashcardSavePath"].asString();
            std::string topicStr(topicBuffer);
            toLowercase(topicStr);
            trim(topicStr);//do more checks to see if topicStr is an appropriate folder name

            //create folder with topic's name
            if (CreateDirectory(std::string(filePath + "/" + topicStr).c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {
                std::ofstream saveFile(filePath + "/" + topicStr + "/" + std::string(fileName) + ".json");

                Json::Value saveJsonRoot;
                Json::StreamWriterBuilder builder;
                const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

                //save meta-data
                saveJsonRoot["topic"] = topicBuffer;
                saveJsonRoot["keywords"] = Json::arrayValue;
                std::string keywordsBufferStr(keywordsBuffer);
                std::stringstream ss(keywordsBufferStr);
                while (ss.good()) {
                    std::string substr;
                    std::getline(ss, substr, ',');
                    trim(substr);
                    toLowercase(substr);
                    if (!substr.empty()) {
                        saveJsonRoot["keywords"].append(substr.c_str());
                    }
                }

                //save boxes
                int i = 0;
                saveJsonRoot["answerBoxPositionsList"] = Json::arrayValue;
                saveJsonRoot["questionBoxPositionsList"] = Json::arrayValue;
                for (auto boxBoundsList : { answerBoxPositions, questionBoxPositions }) {
                    for (std::pair<cv::Point, cv::Point>& boxBounds : boxBoundsList) {
                        Json::Value boxPositions = Json::arrayValue;
                        Json::Value boxTopLeftPosition = Json::arrayValue;
                        boxTopLeftPosition.append(boxBounds.first.x);
                        boxTopLeftPosition.append(boxBounds.first.y);
                        boxPositions.append(boxTopLeftPosition);
                        Json::Value boxBottomRightPosition = Json::arrayValue;
                        boxBottomRightPosition.append(boxBounds.second.x);
                        boxBottomRightPosition.append(boxBounds.second.y);
                        boxPositions.append(boxBottomRightPosition);
                        if (i == 0) {
                            saveJsonRoot["answerBoxPositionsList"].append(boxPositions);
                        }
                        else {
                            saveJsonRoot["questionBoxPositionsList"].append(boxPositions);
                        }
                    }
                    i++;
                }

                writer->write(saveJsonRoot, &saveFile);
                saveFile.close();

                //save image
                if (!cv::imwrite(filePath + "/" + topicStr + "/" + std::string(fileName) + ".png", image)) {
                    std::cerr << "Error saving flashcard image." << std::endl;
                }
            }
        }

        bool openButton = ImGui::Button("Open Image"); ImGui::SameLine();
        if (ImGui::Button("New Flashcard")) {
            makeFileName(fileName);
            image = cv::Mat(400, 800, CV_8UC4, cv::Scalar(255, 255, 255, 255));
            answerBoxPositions.clear();
            questionBoxPositions.clear();
        }

        static bool showPresentFlashcardsWindow = false;
        if (ImGui::Button("Present flashcards")) {
            showPresentFlashcardsWindow = true;
        }
        ImGui::End();

        if (showPresentFlashcardsWindow) {
            ImGui::Begin("Present flashcards");

            static char topicsBuffer[1000];
            static char keywordsBuffer[1000];
            static std::vector<std::string> searchTopics;
            static std::vector<std::string> searchKeywords;
            static std::vector<std::string> foundFlashcardFileNames;
            static std::vector<std::pair<cv::Point, cv::Point>> currentFlashcardAnswerBoxBounds;
            static std::vector<std::pair<cv::Point, cv::Point>> currentFlashcardQuestionBoxBounds;
            static bool showNewFlashcard = true;
            static bool hidingAnswer = true;
            static bool hidingQuestion = false;
            static std::string flashcardKeywordsStr;
            ImGui::InputTextWithHint("Topics", "Filter by topics (seperate with commas)", topicsBuffer, IM_ARRAYSIZE(topicsBuffer),
                ImGuiInputTextFlags_CallbackResize, topicsFilterCallback);
            if (topicsFilterCallbackCalled) {
                std::cout << topicsBuffer << std::endl;
                topicsFilterCallbackCalled = false;
            }
            ImGui::InputTextWithHint("Keywords", "Filter by keywords (seperate with commas)", keywordsBuffer, IM_ARRAYSIZE(keywordsBuffer),
                ImGuiInputTextFlags_CallbackResize, keywordsFilterCallback);
            if (keywordsFilterCallbackCalled) {
                std::string fileSavePath = configRoot["flashcardSavePath"].asString();
                searchKeywords = splitString(keywordsBuffer, ",");
                for (std::string &keyword : searchKeywords) {
                    trim(keyword);
                    toLowercase(keyword);
                }
                foundFlashcardFileNames = searchForFlashcards(fileSavePath, "test topic", searchKeywords);
                keywordsFilterCallbackCalled = false;
                showNewFlashcard = true;
            }
            std::string numFlashcardString = "Flashcards found: ";
            numFlashcardString += std::to_string(foundFlashcardFileNames.size());
            ImGui::Text(numFlashcardString.c_str());

            if (hidingAnswer) {
                if (ImGui::Button("Show answer")) {
                    hidingAnswer = false;
                }
            }
            else {
                if (ImGui::Button("Next flashcard")) {
                    showNewFlashcard = true;
                    hidingAnswer = true;
                }
            }

            //load a random flashcard
            if (showNewFlashcard && !foundFlashcardFileNames.empty()) {
                std::string fileSavePath = configRoot["flashcardSavePath"].asString();
                int ri = rand() % foundFlashcardFileNames.size();
                currentFlashcardImage = loadFlashcardImage(fileSavePath, "test topic", foundFlashcardFileNames[ri]);
                currentFlashcardAnswerBoxBounds.clear();
                currentFlashcardQuestionBoxBounds.clear();
                loadFlashcardBoxBounds(fileSavePath, "test topic", foundFlashcardFileNames[ri],
                    currentFlashcardAnswerBoxBounds, currentFlashcardQuestionBoxBounds);
                showNewFlashcard = false;
            }
            currentFlashcardImage.copyTo(currentFlashcardCanvas);

            //draw boxes
            if (hidingAnswer) {
                for (std::pair<cv::Point, cv::Point>& boxBounds : currentFlashcardAnswerBoxBounds) {
                    cv::rectangle(currentFlashcardCanvas, boxBounds.first, boxBounds.second, cv::Scalar(100, 0, 0, 255), cv::FILLED);
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, currentFlashcardCanvas.cols, currentFlashcardCanvas.rows,
                0, GL_RGBA, GL_UNSIGNED_BYTE, currentFlashcardCanvas.data);
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture)), ImVec2(currentFlashcardCanvas.cols, currentFlashcardCanvas.rows));
            
            ImGui::Text("Current flashcard keywords:");
            ImGui::Button("Back to creating flashcards");

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

        glfwSwapBuffers( window );
    }

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    return 0;
}