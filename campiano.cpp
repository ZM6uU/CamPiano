#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <GL/glut.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define OUT_VIDEO_FILE "rec.avi"
#define PI 3.1415926535
#define SAMPLE_RATE 44100

#define DO_F 523
#define RE_F 587
#define MI_F 659
#define FA_F 699
#define SO_F 784
#define RA_F 880
#define SI_F 987

//whileループの1cycleで出力する振幅の数
int one_cycle_out_length = 1500;

//音の大きさ
int strength = 5000;

// (1/SAMPLE_RATE) が何回着たかのカウント
int g_count = 0;

//音の情報
std::vector<short> out;

//色の変化を検知する
bool detect(int, int);

cv::Mat frame;
//表示するときに左右を反転させ鏡のように表示
cv::Mat mirror_frame;
//はじめの色を記録しておく
cv::Mat static_frame;

//画面上に表示するノーツの数の最大値
int notes_max_num = 7;
int notes_num = 0;

int on_count = 0;
int on_note_id = 0;
//
int circle_radius = 30;
int circle_radius2 = 900;

//ノーツ生成時のノーツ位置のRGBを保存しておくための変数
//vectorなのはノーツが複数なのに対応するため
std::vector<int> sR;
std::vector<int> sG;
std::vector<int> sB;

//ノーツの位置のRGBこの値が更新され上のsR,sG,sBと比較される
std::vector<int> dR;
std::vector<int> dG;
std::vector<int> dB;

//何個ノーツを生成したか
int g_id = 0;

//前方宣言
class Note;

//生成してあるノーツのvector
std::vector<Note> Notes;

int g_stage = 0;
bool g_stage_first_in[7] = {0, 0, 0, 0, 0, 0, 0};
int g_stage_first_count = 0;
int g_stage_check_id[7] = {};
bool g_stage_prepare = true;
int stage_last_note = 100;

//ノーツ
class Note
{
public:
    //ノーツの音によって色を変える。
    enum color {
        RED = 0,     //ド
        ORANGE = 1,  //レ
        YELLOW = 2,  //ミ
        GREEN = 3,   //ファ
        BLUE = 4,    //ソ
        INDIGO = 5,  //ラ
        PURPLE = 6,  //シ
        BLACK = 7,   //それ以外
    };

    void setColor(float f)
    {
        int f_for_color = f;
        switch (f_for_color) {
        case DO_F:
        case DO_F * 2:
        case DO_F * 3:
            m_color = RED;
            break;
        case RE_F:
        case RE_F * 2:
        case RE_F * 3:
            m_color = ORANGE;
            break;
        case MI_F:
        case MI_F * 2:
        case MI_F * 3:
            m_color = YELLOW;
            break;
        case FA_F:
        case FA_F * 2:
        case FA_F * 3:
            m_color = GREEN;
            break;
        case SO_F:
        case SO_F * 2:
        case SO_F * 3:
            m_color = BLUE;
            break;
        case RA_F:
        case RA_F * 2:
        case RA_F * 3:
            m_color = INDIGO;
            break;
        case SI_F:
        case SI_F * 2:
        case SI_F * 3:
            m_color = PURPLE;
            break;
        default:
            m_color = BLACK;
            break;
        }
    }
    //constructor
    Note(float f, int x, int y) : m_freq(f), m_pos_x(x), m_pos_y(y)
    {
        //古いものから消去
        if (notes_num == notes_max_num) {
            (*(Notes.begin())).off();
            Notes.erase(Notes.begin());
            notes_num--;
            for (int i = 0; i < notes_max_num; i++) {
                Notes[i].setColor(Notes[i].getF());
            }
        }
        Notes.push_back(*this);
        m_id = g_id;
        setColor(f);
        int sr = static_frame.at<cv::Vec3b>(y, x)[2];
        int sg = static_frame.at<cv::Vec3b>(y, x)[1];
        int sb = static_frame.at<cv::Vec3b>(y, x)[0];
        int id = (g_id % 7);
        sR[id] = sr;
        sG[id] = sg;
        sB[id] = sb;
        g_id++;
        notes_num++;
    }

    enum color getColer() { return m_color; }
    float getF() { return m_freq; }
    int getX() { return m_pos_x; }
    int getY() { return m_pos_y; }


    void setF(float f)
    {
        m_freq = f;
        setColor(f);
    }

    //ノーツの移動(絶対位置)
    void setX(int x) { m_pos_x = x; }
    void setY(int y) { m_pos_y = y; }

    //ノーツの場所を移動させる相対位置
    void move(int x, int y)
    {
        m_pos_x += x;
        m_pos_y += y;
        resetStaticRGB();
    }

    //ノーツの位置の画素に注目、変化があればノーツの状態をonにして鳴らす
    void check()
    {
        for (int i = 0; i < 7; i++) {
            dR[get7Id()] = frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[2];
            dG[get7Id()] = frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[1];
            dB[get7Id()] = frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[0];
        }

        if (detect(sR[get7Id()], dR[get7Id()]) || detect(sG[get7Id()], dG[get7Id()]) || detect(sB[get7Id()], dB[get7Id()])) {
            this->on();
        }
    }

    void resetStaticRGB()
    {
        int sr = static_frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[2];
        int sg = static_frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[1];
        int sb = static_frame.at<cv::Vec3b>(m_pos_y, m_pos_x)[0];
        sR[get7Id()] = sr;
        sG[get7Id()] = sg;
        sB[get7Id()] = sb;
    }

    void ring()
    {
        if (m_on) {
            for (int i = g_count; i < g_count + one_cycle_out_length; i++) {
                out[i - g_count] += strength * sin(2.0 * PI * m_freq * i / SAMPLE_RATE);
            }
        }
    }

    void draw()
    {
        if (m_on) {
            circle_radius = 40;
            circle_radius2 = 1600;
        }

        for (int i = -circle_radius; i <= circle_radius; i++) {
            for (int j = -circle_radius; j <= circle_radius; j++) {

                if (abs(i * i + j * j) > circle_radius2) {

                } else {

                    switch (m_color) {
                    case Note::RED:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(0, 0, 255.0 * abs(i * i + j * j) / circle_radius2);
                        break;
                    case Note::ORANGE:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(0, 122.5 * abs(i * i + j * j) / circle_radius2, 255.0 * abs(i * i + j * j) / circle_radius2);
                        break;
                    case Note::YELLOW:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(0, 255.0 * abs(i * i + j * j) / circle_radius2, 255.0 * abs(i * i + j * j) / circle_radius2);
                        break;
                    case Note::GREEN:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(0, 255.0 * abs(i * i + j * j) / circle_radius2, 0);
                        break;
                    case Note::BLUE:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(255.0 * abs(i * i + j * j) / circle_radius2, 0, 0);
                        break;
                    case Note::INDIGO:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(255.0 * abs(i * i + j * j) / circle_radius2, 0, 72.5 * abs(i * i + j * j) / circle_radius2);
                        break;
                    case Note::PURPLE:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) += cv::Vec3b(255.0 * abs(i * i + j * j) / circle_radius2, 0, 255.0 * abs(i * i + j * j) / circle_radius2);
                        break;
                    case Note::BLACK:
                        mirror_frame.at<cv::Vec3b>(m_pos_y + j, 640 - (m_pos_x + i)) -= cv::Vec3b::all(255.0 * abs(i * i + j * j) / circle_radius2);
                        break;
                    }
                }
            }
        }
        //globalなので元にもどしておく
        circle_radius = 30;
        circle_radius2 = 900;
    }
    int getTotalId()
    {
        return m_id;
    }

    int get7Id()
    {
        return m_id % 7;
    }
    bool state()
    {
        return m_on;
    }

    void on()
    {
        if (m_on == false) {
            on_count++;
        }
        m_on = true;
    }

    void off()
    {
        if (m_on == true) {
            on_count--;
        }
        m_on = false;
    }

private:
    //ノーツを識別するためのid
    int m_id;

    //ノーツの色(周波数に対応)
    enum color m_color;
    //ノーツを押したときになる音の周波数
    float m_freq;

    //ノーツの位置情報
    int m_pos_x;
    int m_pos_y;

    //ノーツをならすか鳴らさないかフラッグ
    bool m_on = false;
};  //end of class Notes

int block_size = 5;

char key = 'd';


// mode で演奏モードとノーツ移動モードを切り替える。
// 'p'  -> play mode
// 'd' -> defalut play mode
// 'm' -> move mode
// 'r' -> rec mode
int mode = 'p';

//左右反転
void setMirrorframe();


//ノーツの配置関連
void defaultNotes();
void Uewomuite();
void autoUpdate();


int main(int argc, char* argv[])
{
    //動画の再生
    if (argc == 2) {
        cv::VideoCapture cap;
        std::string input_video;
        input_video = argv[1];
        cap.open(input_video);
        cv::namedWindow("video", 1);

        if (!cap.isOpened()) {
            std::cout << "not opened" << std::endl;
        }

        while (1) {
            cap >> frame;
            if (frame.empty()) {
                break;
            }
            imshow("video", frame);
            key = cv::waitKey(33);
        }
        return 0;
    }

    using namespace std;
    cout << "If your background is not static, unexpected sounds may ring." << endl;
    cout << "Please use this app in static environment" << endl;
    cout << "If you are ready, press Y[y] and this app will start." << endl;
    string use_this_app;
    cin >> use_this_app;
    if (use_this_app != "y" && use_this_app != "Y") {
        return 0;
    }

    //領域確保
    sR.reserve(notes_max_num);
    sG.reserve(notes_max_num);
    sB.reserve(notes_max_num);
    dR.reserve(notes_max_num);
    dG.reserve(notes_max_num);
    dB.reserve(notes_max_num);
    Notes.reserve(notes_max_num);

    //フレームの準備
    cv::VideoCapture cap;
    cap.open(1);
    if (!cap.isOpened()) {
        printf("Cannot open the video.\n");
        cap.open(0);
    }
    cv::VideoWriter output_video;
    output_video.open(OUT_VIDEO_FILE, CV_FOURCC('M', 'J', 'P', 'G'), 30, cv::Size(640, 480));

    cap >> static_frame;
    frame = static_frame.clone();
    mirror_frame = static_frame.clone();


    defaultNotes();

    auto one_cycle_start = std::chrono::system_clock::now();
    auto one_cycle_end = std::chrono::system_clock::now();
    double one_cycle_time = std::chrono::duration_cast<std::chrono::milliseconds>(one_cycle_end - one_cycle_start).count();

    FILE* sound;
    sound = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
    while (1) {

        //長さを調整
        out.reserve(one_cycle_out_length);
        //次の長さ調整のために時間の測定
        one_cycle_start = std::chrono::system_clock::now();

        //モード切り替え用
        key = cv::waitKey(10);

        //ノーツ移動用(move)
        if (mode != 'm' && key == 'm') {
            mode = 'm';
        }
        //録画モード(rec)
        if (mode != 'r' && key == 'r') {
            mode = 'r';
        }

        //演奏モード(play (default))
        if (mode != 'd' && key == 'd') {
            mode = 'd';
            g_stage = 0;
            defaultNotes();
        }
        //演奏モードに戻る(fin)
        if (key == 'p') {
            mode = 'p';
        }

        //上を向いて歩こうモード(uewomuite)
        if (mode != 'u' && key == 'u') {
            mode = 'u';
            g_stage = 0;
            Uewomuite();
        }

        //ノーツ自動更新モード(auto)
        if (mode != 'a' && key == 'a') {
            mode = 'a';
        }

        if (mode != 'm' && mode != 'a') {

            //このループの出力をきれいにしとく
            for (int i = 0; i < one_cycle_out_length; i++) {
                out[i] = 0;
            }
            cap >> frame;
            setMirrorframe();

            for (int i = 0; i < notes_max_num; i++) {
                Notes[i].off();
                Notes[i].check();
                Notes[i].ring();
                Notes[i].draw();
            }
            //録画用
            if (mode == 'r') {
                output_video << mirror_frame;
                for (int i = 0; i < mirror_frame.size().width; i++) {
                    for (int j = 0; j < mirror_frame.size().height; j++) {
                        if (i == 0 || j == 0 || i == mirror_frame.size().width - 1 || j == mirror_frame.size().width - 1) {
                            mirror_frame.at<cv::Vec3b>(j, i) = cv::Vec3b(0, 0, 255);
                        }
                    }
                }
            }

            //出力
            fwrite(&out[0], 2, one_cycle_out_length, sound);
        } else if (mode == 'm') {
            cap >> frame;
            setMirrorframe();
            for (int i = 0; i < notes_max_num; i++) {
                Notes[i].off();
                if (on_count == 0) {
                    Notes[i].check();
                    on_note_id = i;
                }
                Notes[i].draw();
            }
            if (Notes[on_note_id].state()) {
                if (key == 'Q') {
                    Notes[on_note_id].move(1, 0);
                } else if (key == 'R') {
                    Notes[on_note_id].move(0, -1);
                } else if (key == 'T') {
                    Notes[on_note_id].move(0, 1);
                } else if (key == 'S') {
                    Notes[on_note_id].move(-1, 0);
                }
            }
            Notes[on_note_id].resetStaticRGB();

        } else if (mode == 'a') {
            //このループの出力をきれいにしとく
            for (int i = 0; i < one_cycle_out_length; i++) {
                out[i] = 0;
            }
            cap >> frame;
            setMirrorframe();
            autoUpdate();
            for (int i = 0; i < notes_max_num; i++) {
                Notes[i].off();
                Notes[i].check();
                Notes[i].ring();
                Notes[i].draw();
            }
            fwrite(&out[0], 2, one_cycle_out_length, sound);
        }


        //ちょっと大きく表示したいので一瞬だけresize
        cv::resize(mirror_frame, mirror_frame, cv::Size(960, 720));
        cv::namedWindow("Input Image", 1);
        cv::imshow("Input Image", mirror_frame);
        cv::resize(mirror_frame, mirror_frame, cv::Size(640, 480));


        //終了するとき
        if (key == 27 || key == 'q') {
            break;
        }

        //次の周期に必要なデータの更新
        g_count += one_cycle_out_length;
        //次の長さ調整
        one_cycle_end = std::chrono::system_clock::now();
        one_cycle_time = std::chrono::duration_cast<std::chrono::milliseconds>(one_cycle_end - one_cycle_start).count();
        one_cycle_out_length = one_cycle_time / ((1.0 / SAMPLE_RATE) * 1000.0);
    }

    pclose(sound);
    return 0;
}


//aとbが離れているとtrueを返す
bool detect(int a, int b)
{
    if (abs(a - b) < 50) {
        return false;
    }
    return true;
}

void setMirrorframe()
{
    for (int x = 0; x < frame.size().width; x++) {
        for (int y = 0; y < frame.size().height; y++) {
            mirror_frame.at<cv::Vec3b>(y, x) = frame.at<cv::Vec3b>(y, frame.size().width - 1 - x);
        }
    }
}

void defaultNotes()
{
    std::vector<float> default_f;
    std::vector<int> default_pos_x;
    std::vector<int> default_pos_y;

    default_f.reserve(notes_max_num);
    default_pos_x.reserve(notes_max_num);
    default_pos_y.reserve(notes_max_num);

    //D
    default_f[0] = DO_F * 2;
    default_pos_x[0] = -320 * cos(3.14 * 8.0 / 10) + 320;
    default_pos_y[0] = -320 * sin(3.14 * 8.0 / 10) + 380;
    //Re
    default_f[1] = RE_F * 2;
    default_pos_x[1] = -320 * cos(3.14 * 7.0 / 10) + 320;
    default_pos_y[1] = -320 * sin(3.14 * 7.0 / 10) + 380;
    //Mi
    default_f[2] = MI_F * 2;
    default_pos_x[2] = -320 * cos(3.14 * 6.0 / 10) + 320;
    default_pos_y[2] = -320 * sin(3.14 * 6.0 / 10) + 380;

    //Fa
    default_f[3] = FA_F * 2;
    default_pos_x[3] = -320 * cos(3.14 * 5.0 / 10) + 320;
    default_pos_y[3] = -320 * sin(3.14 * 5.0 / 10) + 380;
    //So
    default_f[4] = SO_F * 2;
    default_pos_x[4] = -320 * cos(3.14 * 4.0 / 10) + 320;
    default_pos_y[4] = -320 * sin(3.14 * 4.0 / 10) + 380;
    //Ra
    default_f[5] = RA_F * 2;
    default_pos_x[5] = -320 * cos(3.14 * 3.0 / 10) + 320;
    default_pos_y[5] = -320 * sin(3.14 * 3.0 / 10) + 380;
    //Si
    default_f[6] = SI_F * 2;
    default_pos_x[6] = -320 * cos(3.14 * 2.0 / 10) + 320;
    default_pos_y[6] = -320 * sin(3.14 * 2.0 / 10) + 380;

    for (int i = 0; i < notes_max_num; i++) {
        Note default_note(default_f[i], default_pos_x[i], default_pos_y[i]);
    }

    Note default_note(default_f[0], default_pos_x[0], default_pos_y[0]);
    for (int i = 0; i < notes_max_num; i++) {
        Notes[i].resetStaticRGB();
        Notes[i].setColor(Notes[i].getF());
    }
}

void Uewomuite()
{
    std::vector<float> uewomuite_f;
    std::vector<int> uewomuite_pos_x;
    std::vector<int> uewomuite_pos_y;

    uewomuite_f.reserve(notes_max_num);
    uewomuite_pos_x.reserve(notes_max_num);
    uewomuite_pos_y.reserve(notes_max_num);

    //Do
    uewomuite_f[0] = DO_F * 2;
    uewomuite_pos_x[0] = -320 * cos(3.14 * 6.0 / 10) + 320;
    uewomuite_pos_y[0] = -320 * sin(3.14 * 6.0 / 10) + 380;
    //Re
    uewomuite_f[1] = RE_F * 2;
    uewomuite_pos_x[1] = -320 * cos(3.14 * 5.0 / 10) + 320;
    uewomuite_pos_y[1] = -320 * sin(3.14 * 5.0 / 10) + 380;
    //Mi
    uewomuite_f[2] = MI_F * 2;
    uewomuite_pos_x[2] = -320 * cos(3.14 * 4.0 / 10) + 320;
    uewomuite_pos_y[2] = -320 * sin(3.14 * 4.0 / 10) + 380;

    //Low_r
    uewomuite_f[3] = RA_F;
    uewomuite_pos_x[3] = -320 * cos(3.14 * 7.0 / 10) + 320;
    uewomuite_pos_y[3] = -320 * sin(3.14 * 7.0 / 10) + 380;
    //So
    uewomuite_f[4] = 1568;
    uewomuite_pos_x[4] = -320 * cos(3.14 * 3.0 / 10) + 320;
    uewomuite_pos_y[4] = -320 * sin(3.14 * 3.0 / 10) + 380;
    //Low_s
    uewomuite_f[5] = SO_F;
    uewomuite_pos_x[5] = -320 * cos(3.14 * 8.0 / 10) + 320;
    uewomuite_pos_y[5] = -320 * sin(3.14 * 8.0 / 10) + 380;
    //Ra
    uewomuite_f[6] = 1760;
    uewomuite_pos_x[6] = -320 * cos(3.14 * 2.0 / 10) + 320;
    uewomuite_pos_y[6] = -320 * sin(3.14 * 2.0 / 10) + 380;

    Notes.reserve(notes_max_num);
    for (int i = 0; i < notes_max_num; i++) {
        Note uewomuite_note(uewomuite_f[i], uewomuite_pos_x[i], uewomuite_pos_y[i]);
    }
    for (int i = 0; i < notes_max_num; i++) {
        Notes[i].resetStaticRGB();
        Notes[i].setColor(Notes[i].getF());
    }
}

bool alltrue(bool a[7])
{
    for (int i = 0; i < notes_max_num; i++) {
        if (a[i] == false) {
            return false;
        }
    }
    return true;
}

void autoUpdate()
{

    std::vector<float> auto_f;
    std::vector<int> auto_pos_x;
    std::vector<int> auto_pos_y;
    auto_f.reserve(notes_max_num);
    auto_pos_x.reserve(notes_max_num);
    auto_pos_y.reserve(notes_max_num);
    auto_pos_x[0] = 530;
    auto_pos_x[1] = 470;
    auto_pos_x[2] = 410;
    auto_pos_x[3] = 350;
    auto_pos_x[4] = 290;
    auto_pos_x[5] = 230;
    auto_pos_x[6] = 170;
    auto_pos_y[0] = 150;
    auto_pos_y[1] = 150;
    auto_pos_y[2] = 150;
    auto_pos_y[3] = 150;
    auto_pos_y[4] = 150;
    auto_pos_y[5] = 150;
    auto_pos_y[6] = 150;

    if (g_stage == 0 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = RE_F * 2;
        auto_f[3] = MI_F * 2;
        auto_f[4] = DO_F * 2;
        auto_f[5] = RA_F;
        auto_f[6] = SO_F;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 1 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = RE_F * 2;
        auto_f[3] = MI_F * 2;
        auto_f[4] = DO_F * 2;
        auto_f[5] = RA_F;
        auto_f[6] = SO_F;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 2 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = RE_F * 2;
        auto_f[3] = MI_F * 2;
        auto_f[4] = MI_F * 2;
        auto_f[5] = SO_F * 2;
        auto_f[6] = RA_F * 2;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 3 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = RA_F * 2;
        auto_f[1] = SO_F * 2;
        auto_f[2] = RA_F * 2;
        auto_f[3] = SO_F * 2;
        auto_f[4] = MI_F * 2;
        auto_f[5] = RE_F * 2;
        auto_f[6] = DO_F * 2;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 4 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = RA_F;
        auto_f[3] = RE_F * 2;
        auto_f[4] = RE_F * 2;
        auto_f[5] = DO_F * 2;
        auto_f[6] = MI_F * 2;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 5 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = RA_F * 2;
        auto_f[3] = SO_F * 2;
        auto_f[4] = MI_F * 2;
        auto_f[5] = DO_F * 2;
        auto_f[6] = RA_F;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    } else if (g_stage == 5 && g_stage_prepare) {
        stage_last_note = 100;
        g_stage_prepare = false;
        auto_f[0] = DO_F * 2;
        auto_f[1] = DO_F * 2;
        auto_f[2] = DO_F * 2;
        auto_f[3] = DO_F * 2;
        auto_f[4] = DO_F * 2;
        auto_f[5] = DO_F * 2;
        auto_f[6] = DO_F * 2;
        Notes.reserve(notes_max_num);
        for (int i = 0; i < notes_max_num; i++) {
            Note auto_note(auto_f[i], auto_pos_x[i], auto_pos_y[i]);
            g_stage_check_id[i] = auto_note.getTotalId();
        }
        for (int i = 0; i < notes_max_num; i++) {
            Notes[i].resetStaticRGB();
        }
    }
    for (int i = 0; i < notes_max_num; i++) {
        if ((Notes[i].state() && i != stage_last_note) || (Notes[i].state() && (i == stage_last_note && g_stage_first_in[5] == 1))) {
            if (g_stage_first_in[5] == 0) {
                g_stage_first_in[6] = 0;
            }
            if (Notes[i].get7Id() == 6) {
                g_stage_first_count++;
                g_stage_first_in[g_stage_first_count] = true;
                //すべて押されたなら次のステージにGO
                if (g_stage_first_in[6]) {
                    stage_last_note = i;
                    break;
                }
            }
        }
    }

    if (stage_last_note != 100 && !Notes[stage_last_note].state()) {
        g_stage_first_count = 0;
        for (int j = 0; j < notes_max_num; j++) {
            g_stage_first_in[j] = false;
        }
        g_stage++;
        g_stage_prepare = true;
    }
    for (int i = 0; i < notes_max_num; i++) {
        Notes[i].setColor(Notes[i].getF());
    }
}
