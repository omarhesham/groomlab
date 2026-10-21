// SPDX-License-Identifier: Apache-2.0
#pragma once

// What the app's standalone renders share, whatever the renderer (V-Ray Standalone: vray_render.h;
// RenderMan's prman: prman_render.h): the stage the viewport hands them (camera, lighting, scalp,
// size, turntable), the renderer's process with its progress read from its console (RenderJob, a
// RenderProgress per renderer), and the window that shows the image as it comes (RenderWindow).
// Each renderer writes its own scene from the stage and says how its console reads.

#include <QImage>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QWidget>

#include <array>
#include <memory>
#include <string>

#include "look.h"
#include "groomlab/graph.h"  // MeshPtr

class QLabel;
class QProcess;
class QProgressBar;
class QPushButton;

namespace groomlab::app {

struct RenderStage {
    QVector3D eye{0.0f, 0.0f, 10.0f}, target{0.0f, 0.0f, 0.0f};
    float fovDegrees = 40.0f;  // vertical, as the viewport's camera
    LightingSetup lighting;
    MeshPtr scalp;  // null or hidden: none
    bool showScalp = true;
    RGB scalpColor{0.36f, 0.29f, 0.25f};  // linear
    int width = 1280, height = 720;
    int turntableFrames = 0;  // 0: one frame; else this many frames around the target
    // V-Ray's sampling: the adaptive sampler's max subdivs and noise threshold, brute-force GI's depth.
    int maxSubdivs = 8;
    float threshold = 0.01f;
    int giDepth = 8;
    // RenderMan's: the hider's max and min samples, the pixel variance, the path tracer's bounces.
    int maxSamples = 128, minSamples = 16;
    float pixelVariance = 0.02f;
    int bounces = 10;
};

// ---- The stage's pieces both scene writers use ----------------------------------------------------

using V3 = std::array<float, 3>;
// The camera looking from `eye` at `target`, Y up: its right, up and forward (toward the target).
struct CameraAxes {
    V3 right, up, forward;
};
CameraAxes lookAt(const V3& eye, const V3& target);
// `eye` turned around `target` about Y by `degrees` (a turntable's frame k of n: 360 k / n).
V3 orbitEye(const V3& eye, const V3& target, float degrees);
// The lighting's environment map (makeEnvironment), turned by its environmentRotation.
EnvironmentMap stageEnvironment(const LightingSetup& lighting, int width = 256, int height = 128);
// A Radiance .hdr (RGBE, flat) of the map: row 0 the zenith, as EnvironmentMap. Throws
// std::runtime_error when it cannot be written.
void writeHdr(const std::string& path, const EnvironmentMap& map);
// A number as scenes write it: 7 significant digits, tiny values 0.
std::string sceneNumber(double v);

// The image a render writes for frame `frame` (1-based) of `imagePath`: the path itself for one
// frame, "<stem>.0001.<ext>"... for a turntable.
QString renderFrameImage(const QString& imagePath, int frame, int turntableFrames);

// ---- The renderer's process -----------------------------------------------------------------------

// A renderer's console as it goes: feed() takes a line, true when it said something new.
struct RenderProgress {
    int frame = 0;          // the frame being rendered (1-based), 0 before the first
    double percent = 0.0;   // of its image
    QString written;        // the last image written ("" when the renderer does not say)
    int imagesWritten = 0;
    QString error;          // the first error line
    virtual ~RenderProgress() = default;
    virtual bool feed(const QString& line) = 0;
};

// One render: the renderer on a scene, its progress, the images as they are written.
class RenderJob : public QObject {
    Q_OBJECT
public:
    explicit RenderJob(QObject* parent = nullptr);
    ~RenderJob() override;
    // `name` for messages ("V-Ray"); `environment` empty: the app's own.
    bool start(const QString& name, const QString& program, const QStringList& arguments,
               const QProcessEnvironment& environment, const QString& workingDirectory, const QString& imagePath,
               int turntableFrames, std::unique_ptr<RenderProgress> progress);
    void cancel();
    bool running() const;
    // Blocks until it ends (scripted runs); false when it failed or was cancelled.
    bool wait();
    const QString& imagePath() const { return image_; }
    const QString& name() const { return name_; }
    int frames() const { return frames_; }

signals:
    void progress(double fraction, const QString& text);
    void imageWritten(const QString& path);
    void finished(bool ok, const QString& message);

private:
    void read();
    void done(int exitCode);

    QProcess* process_ = nullptr;
    QString name_, image_;
    int frames_ = 1, turntableFrames_ = 0;
    bool cancelled_ = false, ended_ = false, ok_ = false;
    QByteArray pending_;
    std::unique_ptr<RenderProgress> state_;
};

// The render's window (never modal): the image as it comes, the progress, Cancel, Save Image...
// and Show in Folder.
class RenderWindow : public QWidget {
    Q_OBJECT
public:
    explicit RenderWindow(QWidget* parent = nullptr);
    void begin(RenderJob* job, const QString& title);
    void showImage(const QString& path);
    QImage image() const { return image_; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void fit();

    RenderJob* job_ = nullptr;
    QLabel* view_ = nullptr;
    QLabel* status_ = nullptr;
    QProgressBar* bar_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* folder_ = nullptr;
    QImage image_;
    QString imagePath_;
};

}  // namespace groomlab::app
