// SPDX-License-Identifier: Apache-2.0
#include "photos_panel.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "groomlab/reference_edit.h"

#include "glass.h"
#include "groom_document.h"
#include "list_bar.h"
#include "theme.h"

namespace groomlab::app {

namespace {

using nlohmann::json;

const ui::Definitions& defs() { return ui::Definitions::shipped(); }
QString qs(const std::string& s) { return QString::fromStdString(s); }

json cameraJson(const RefCamera& c) {
    json rot = json::array();
    for (const auto& row : c.pose.rotation) rot.push_back({row[0], row[1], row[2]});
    return {{"rotation", rot}, {"position", {c.pose.position[0], c.pose.position[1], c.pose.position[2]}}, {"focal_px", c.focalPx}};
}

std::optional<RefCamera> cameraFrom(const json& j, int width, int height) {
    if (!j.is_object() || !j.contains("rotation") || !j.contains("position")) return std::nullopt;
    try {
        RefCamera c;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t k = 0; k < 3; ++k) c.pose.rotation[i][k] = j["rotation"].at(i).at(k).get<double>();
        for (std::size_t i = 0; i < 3; ++i) c.pose.position[i] = j["position"].at(i).get<double>();
        c.focalPx = j.value("focal_px", 0.0);
        c.width = width;
        c.height = height;
        return c;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

ReferencePhotoEditor::State stateOf(const json& photo) {
    try {
        return referencePhotoStateFromJson(photo.contains("state") && photo["state"].is_object() ? photo["state"].dump() : std::string());
    } catch (const std::exception&) {
        return {};
    }
}

// The focal length guessed for a photo never lined up: a 50 mm lens on a 36 mm film back.
double focalGuess(double width) { return std::max(width, 1.0) * 50.0 / 36.0; }

QToolButton* chip(const QString& text, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setProperty("role", "chip");
    b->setCheckable(true);
    return b;
}

QLabel* hint(QWidget* parent, const char* role = "hint") {
    auto* l = new QLabel(parent);
    l->setProperty("role", role);
    l->setWordWrap(true);
    return l;
}

}  // namespace

PhotosPanel::PhotosPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("photosPanel"));
    const ui::json& lists = defs().get("lists/photos");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);
    about_ = aboutLabel(lists.value("about", std::string()), this);
    layout->addWidget(about_);
    bar_ = new ListBar(this);
    layout->addWidget(bar_);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("referenceList"));
    list_->setIconSize(QSize(48, 36));
    list_->setMinimumHeight(96);
    list_->setMaximumHeight(132);
    list_->setToolTip(tr("The reference photos. Double-click: look through it; right-click: more"));
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int) {
        if (!updating_) refreshControls();
    });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        if (current() >= 0) emit lookThroughRequested(current(), true);
    });
    connect(list_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (QMenu* menu = contextMenu(pos)) menu->popup(list_->viewport()->mapToGlobal(pos));
    });
    layout->addWidget(list_);

    // How it shows: looking through, locked, the photo's opacity, side and visibility.
    auto* row = new QHBoxLayout();
    row->setSpacing(6);
    look_ = new QPushButton(tr("Look Through"), this);
    look_->setObjectName(QStringLiteral("referenceLook"));
    look_->setCheckable(true);
    look_->setToolTip(tr("Show the photo's camera in the viewport (again: back to the camera it showed before)"));
    connect(look_, &QPushButton::clicked, this, [this](bool on) {
        if (current() >= 0) emit lookThroughRequested(current(), on);
    });
    row->addWidget(look_, 1);
    lock_ = chip(tr("Lock"), tr("Lock the camera where it is: its position, rotation, focal length and the photo on it (tumbling through it "
                                "does nothing)"),
                 this);
    lock_->setObjectName(QStringLiteral("referenceLock"));
    connect(lock_, &QToolButton::toggled, this, [this](bool on) {
        if (updating_ || current() < 0) return;
        Display d = display(current());
        d.locked = on;
        setDisplay(current(), d);
    });
    row->addWidget(lock_);
    layout->addLayout(row);

    auto* form = new QHBoxLayout();
    form->setSpacing(6);
    auto* opacityLabel = new QLabel(tr("Opacity"), this);
    opacityLabel->setProperty("role", "propLabel");
    form->addWidget(opacityLabel);
    opacity_ = new ValueSlider(this);
    opacity_->setObjectName(QStringLiteral("referenceOpacity"));
    opacity_->setRange(0.0, 1.0);
    opacity_->setLimits(0.0, 1.0);
    opacity_->setDecimals(2);
    opacity_->setStep(0.05);
    opacity_->setValue(0.6);
    opacity_->setTrackWidth(0);
    opacity_->setFill(theme::blue());
    opacity_->setToolTip(tr("How opaque the photo is: see the mesh through it"));
    opacity_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(opacity_, &ValueSlider::valueChanged, this, [this](double v) {
        if (updating_ || current() < 0) return;
        Display d = display(current());
        d.opacity = v;
        setDisplay(current(), d);
    });
    form->addWidget(opacity_, 1);
    layout->addLayout(form);

    // The photo camera's focal length (mm on a 36 mm film back), by hand: Keep focal length turns on,
    // so a solve keeps it. A locked camera keeps its own.
    auto* focalRow = new QHBoxLayout();
    focalRow->setSpacing(6);
    auto* focalLabel = new QLabel(tr("Focal length"), this);
    focalLabel->setProperty("role", "propLabel");
    focalRow->addWidget(focalLabel);
    focal_ = new ValueSlider(this);
    focal_->setObjectName(QStringLiteral("referenceFocal"));
    focal_->setRange(10.0, 200.0);
    focal_->setLimits(2.5, 3500.0);
    focal_->setDecimals(1);
    focal_->setStep(1.0);
    focal_->setValue(50.0);
    focal_->setTrackWidth(0);
    focal_->setFill(theme::blue());
    focal_->setToolTip(tr("The photo camera's focal length (mm): Solve works it out from the point pairs; set it by hand when you know "
                          "the lens (Keep focal length turns on, so a solve keeps it). A locked camera keeps its own"));
    focal_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(focal_, &ValueSlider::valueChanged, this, [this](double mm) {
        const int i = current();
        if (updating_ || i < 0 || display(i).locked) return;
        const QSize size = imageSize(i);
        std::optional<RefCamera> c = camera(i);
        if (!c && placeCamera(i)) c = camera(i);
        if (!c || size.isEmpty()) return;
        c->focalPx = mm * size.width() / 36.0;
        setCamera(i, *c);
        if (!keepFocal_->isChecked()) keepFocal_->setChecked(true);  // (a solve keeps the focal length set by hand)
        if (!looking_.isEmpty() && name(i) == looking_) emit lookThroughRequested(i, true);  // the view through it, again
        refreshPairs();
    });
    focalRow->addWidget(focal_, 1);
    layout->addLayout(focalRow);

    auto* sides = new QHBoxLayout();
    sides->setSpacing(4);
    const ui::json& depth = defs().get("choices/photoDepth");
    auto* depthLabel = new QLabel(qs(depth.value("label", std::string("Photo"))), this);
    depthLabel->setProperty("role", "propLabel");
    sides->addWidget(depthLabel);
    for (const ui::json& c : depth.at("items")) {
        const bool isFront = c.value("value", std::string()) == "front";
        QToolButton* b = chip(qs(c.value("label", std::string())), qs(c.value("tip", std::string())), this);
        b->setObjectName(QStringLiteral("referenceDepth_") + qs(c.value("value", std::string())));
        b->setAutoExclusive(true);
        connect(b, &QToolButton::clicked, this, [this, isFront] {
            if (updating_ || current() < 0) return;
            Display d = display(current());
            d.front = isFront;
            setDisplay(current(), d);
        });
        (isFront ? front_ : behind_) = b;
        sides->addWidget(b);
    }
    sides->addStretch(1);
    visible_ = new QToolButton(this);  // the eye, as the groom's in the header
    visible_->setObjectName(QStringLiteral("referenceVisible"));
    visible_->setCheckable(true);
    visible_->setChecked(true);
    visible_->setAutoRaise(true);
    visible_->setProperty("role", "glyph");
    visible_->setIconSize(QSize(16, 16));
    visible_->setFixedSize(26, 26);
    {
        QIcon eye;
        eye.addPixmap(tintedIcon(QStringLiteral(":/groomlab/icons/ui_eye.svg"), theme::textMuted(), 16, devicePixelRatioF()), QIcon::Normal, QIcon::Off);
        eye.addPixmap(tintedIcon(QStringLiteral(":/groomlab/icons/ui_eye.svg"), theme::text(), 16, devicePixelRatioF()), QIcon::Normal, QIcon::On);
        visible_->setIcon(eye);
    }
    visible_->setToolTip(tr("Show or hide the photo (the camera stays)"));
    connect(visible_, &QToolButton::toggled, this, [this](bool on) {
        if (updating_ || current() < 0) return;
        Display d = display(current());
        d.visible = on;
        setDisplay(current(), d);
    });
    sides->addWidget(visible_);
    layout->addLayout(sides);

    // Align by points: pairs of a feature on the photo and the same feature on the mesh.
    auto* heading = new QLabel(tr("ALIGN BY POINTS"), this);
    heading->setProperty("role", "subheading");
    layout->addWidget(heading);
    alignAbout_ = hint(this);
    layout->addWidget(alignAbout_);
    row = new QHBoxLayout();
    row->setSpacing(6);
    pick_ = new QToolButton(this);
    pick_->setText(tr("Pick Points..."));
    pick_->setObjectName(QStringLiteral("referencePick"));
    pick_->setProperty("role", "chip");
    pick_->setToolTip(tr("Pick the pairs side by side in Guides from Photos: a click on the photo and one on the mesh (either order) make "
                         "a pair; they fill in order and show here"));
    connect(pick_, &QToolButton::clicked, this, [this] {
        if (current() >= 0) emit pickPointsRequested(current());
    });
    row->addWidget(pick_);
    keepFocal_ = new QCheckBox(tr("Keep focal length"), this);
    keepFocal_->setObjectName(QStringLiteral("referenceKeepFocal"));
    keepFocal_->setToolTip(tr("Solve the position and rotation only, keeping the camera's focal length (4 pairs are enough; else 6, and "
                              "the focal length is solved too)"));
    connect(keepFocal_, &QCheckBox::toggled, this, [this] { refreshPairs(); });
    row->addWidget(keepFocal_);
    row->addStretch(1);
    layout->addLayout(row);
    pairs_ = new QTreeWidget(this);
    pairs_->setObjectName(QStringLiteral("referencePairs"));
    pairs_->setHeaderLabels({tr("#"), tr("Photo (px)"), tr("Mesh"), tr("Error")});
    pairs_->setRootIsDecorated(false);
    pairs_->header()->setStretchLastSection(false);
    pairs_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    pairs_->setMinimumHeight(96);
    pairs_->setMaximumHeight(180);
    pairs_->setToolTip(tr("Point pairs: a point on the photo and the same point on the mesh. Right-click: pick a point again, or delete "
                          "the pair"));
    pairs_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(pairs_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (QMenu* menu = pairsMenu(pos)) menu->popup(pairs_->viewport()->mapToGlobal(pos));
    });
    layout->addWidget(pairs_);
    pairsBar_ = new ListBar(this);
    layout->addWidget(pairsBar_);
    result_ = hint(this);
    result_->setObjectName(QStringLiteral("referenceResult"));
    layout->addWidget(result_);

    // Guides from Photos (experimental): the Reference window does the work.
    heading = new QLabel(tr("GUIDES FROM PHOTOS (EXPERIMENTAL)"), this);
    heading->setProperty("role", "subheading");
    layout->addWidget(heading);
    guidesHint_ = hint(this);
    guidesHint_->setObjectName(QStringLiteral("referenceGuidesHint"));
    layout->addWidget(guidesHint_);
    guidesBar_ = new ListBar(this);
    layout->addWidget(guidesBar_);
    layout->addStretch(1);
    refresh();
}

void PhotosPanel::setDocument(GroomDocument* document) {
    document_ = document;
    looking_.clear();
    refresh();
}

void PhotosPanel::setActions(const ui::Bindings& bindings, const ui::FillOptions& options) {
    bindings_ = bindings;
    options_ = options;
    options_.liveShortcuts = false;
    bar_->build(defs().get("lists/photos/bar"), bindings_, options_);
    pairsBar_->build(defs().get("lists/photos/pairsBar"), bindings_, options_);
    guidesBar_->build(defs().get("lists/photos/guidesBar"), bindings_, options_);
    if (QToolButton* b = bar_->button("photo.import")) b->setObjectName(QStringLiteral("referenceImport"));
    if (QToolButton* b = pairsBar_->button("pairs.solve")) b->setObjectName(QStringLiteral("referenceSolve"));
    if (QToolButton* b = guidesBar_->button("photos.guides")) b->setObjectName(QStringLiteral("referenceOpenWindow"));
}

json PhotosPanel::photos() const {
    if (!document_ || document_->referencePhotos().empty()) return json::array();
    json all = json::parse(document_->referencePhotos(), nullptr, false);
    return all.is_array() ? all : json::array();
}

void PhotosPanel::store(const json& all) {
    if (!document_) return;
    document_->setReferencePhotos(all.empty() ? std::string() : all.dump());
    emit edited();
}

int PhotosPanel::count() const { return static_cast<int>(photos().size()); }

int PhotosPanel::current() const {
    const int row = list_->currentRow();
    return row >= 0 && row < count() ? row : -1;
}

void PhotosPanel::select(int index) {
    if (index < 0 || index >= list_->count()) return;
    list_->setCurrentRow(index);
    refreshControls();
}

QString PhotosPanel::name(int index) const {
    const json all = photos();
    return index >= 0 && index < static_cast<int>(all.size()) ? qs(all[index].value("name", std::string())) : QString();
}

QString PhotosPanel::imagePath(int index) const {
    const json all = photos();
    return index >= 0 && index < static_cast<int>(all.size()) ? qs(all[index].value("image", std::string())) : QString();
}

QSize PhotosPanel::imageSize(int index) const {
    const json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return {};
    return QSize(all[index].value("width", 0), all[index].value("height", 0));
}

QImage PhotosPanel::picture(int index) const {
    const QString path = imagePath(index);
    if (path.isEmpty()) return {};
    const QString key = path + QLatin1Char('|') + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
    auto it = pictures_.find(key);
    if (it == pictures_.end()) it = pictures_.insert(key, QImage(path));
    return *it;
}

std::optional<RefCamera> PhotosPanel::camera(int index) const {
    const json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return std::nullopt;
    return cameraFrom(all[index].value("camera", json()), all[index].value("width", 0), all[index].value("height", 0));
}

void PhotosPanel::setCamera(int index, const RefCamera& c) {
    json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return;
    all[index]["camera"] = cameraJson(c);
    store(all);
}

PhotosPanel::Display PhotosPanel::display(int index) const {
    Display d;
    const json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return d;
    const json j = all[index].value("display", json::object());
    if (!j.is_object()) return d;
    d.opacity = std::clamp(j.value("opacity", d.opacity), 0.0, 1.0);
    d.front = j.value("depth", std::string("front")) != "behind";
    d.locked = j.value("locked", false);
    d.visible = j.value("visible", true);
    return d;
}

void PhotosPanel::setDisplay(int index, const Display& d) {
    json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return;
    all[index]["display"] = {{"opacity", d.opacity}, {"depth", d.front ? "front" : "behind"}, {"locked", d.locked}, {"visible", d.visible}};
    store(all);
    refreshControls();
    emit displayChanged();
}

QString PhotosPanel::uniqueName(const QString& wanted, int except) const {
    const json all = photos();
    auto taken = [&](const QString& n) {
        for (int i = 0; i < static_cast<int>(all.size()); ++i) {
            if (i != except && qs(all[i].value("name", std::string())) == n) return true;
        }
        return false;
    };
    QString name = wanted.trimmed().isEmpty() ? QStringLiteral("photo") : wanted.trimmed();
    for (int k = 2; taken(name); ++k) name = wanted.trimmed() + QStringLiteral("_%1").arg(k);
    return name;
}

int PhotosPanel::importPhoto(const QString& path) {
    if (!document_) {
        emit message(tr("Open a groom first"));
        return -1;
    }
    const QImage image(path);
    if (image.isNull()) {
        emit message(tr("Cannot read the image %1").arg(path));
        return -1;
    }
    json photo = {{"name", uniqueName(QFileInfo(path).completeBaseName()).toStdString()},
                  {"image", QFileInfo(path).absoluteFilePath().toStdString()},
                  {"width", image.width()},
                  {"height", image.height()},
                  {"mask", nullptr}};
    photo["camera"] = cameraJson(placedCamera(image.width(), image.height()));
    photo["state"] = json::parse(referencePhotoStateToJson({}));
    photo["display"] = {{"opacity", 0.6}, {"depth", "front"}, {"locked", false}, {"visible", true}};
    json all = photos();
    all.push_back(photo);
    store(all);
    refresh();
    select(static_cast<int>(all.size()) - 1);
    return static_cast<int>(all.size()) - 1;
}

bool PhotosPanel::rename(int index, const QString& wanted) {
    json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size()) || wanted.trimmed().isEmpty()) return false;
    const QString name = wanted.trimmed();
    if (uniqueName(name, index) != name) {
        emit message(tr("Another photo is named %1").arg(name));
        return false;
    }
    const bool looking = looking_ == qs(all[index].value("name", std::string()));
    all[index]["name"] = name.toStdString();
    store(all);
    if (looking) looking_ = name;
    refresh();
    select(index);
    return true;
}

bool PhotosPanel::remove(int index) {
    json all = photos();
    if (index < 0 || index >= static_cast<int>(all.size())) return false;
    if (qs(all[index].value("name", std::string())) == looking_) emit lookThroughRequested(index, false);
    all.erase(all.begin() + index);
    store(all);
    refresh();
    if (count() > 0) select(std::min(index, count() - 1));
    return true;
}

RefCamera PhotosPanel::placedCamera(int width, int height) const {
    // In front (+Z) of the groom, far enough for it to fill most of the photo's short side (Maya's
    // reference._place), looking down -Z, a 50 mm lens.
    const Bounds b = document_ ? document_->sceneBounds() : Bounds();
    RefCamera c;
    c.width = width;
    c.height = height;
    c.focalPx = focalGuess(width);
    QVector3D center(0.0f, 0.0f, 0.0f);
    float radius = 1.0f;
    if (b.valid) {
        center = QVector3D(b.lo.x + b.hi.x, b.lo.y + b.hi.y, b.lo.z + b.hi.z) * 0.5f;
        radius = std::max(QVector3D(b.hi.x - b.lo.x, b.hi.y - b.lo.y, b.hi.z - b.lo.z).length() * 0.5f, 1e-3f);
    }
    const double half = std::min(std::atan(0.5 * std::max(width, 1) / c.focalPx), std::atan(0.5 * std::max(height, 1) / c.focalPx));
    const double distance = radius / std::sin(half) * 1.05;
    c.pose.position = {center.x(), center.y(), center.z() + distance};
    return c;
}

bool PhotosPanel::placeCamera(int index) {
    const QSize size = imageSize(index);
    if (index < 0 || index >= count() || size.isEmpty()) return false;
    setCamera(index, placedCamera(size.width(), size.height()));
    return true;
}

// ---- Align by Points ------------------------------------------------------------------------------

int PhotosPanel::pairCount() const {
    const json all = photos();
    const int i = current();
    if (i < 0) return 0;
    const MarkerLists& l = stateOf(all[i]).landmarks;
    return static_cast<int>(std::max(l.count(kLandmarkPhoto), l.count(kLandmarkMesh)));
}

int PhotosPanel::pickedPair() const {
    const int row = pairs_->indexOfTopLevelItem(pairs_->currentItem());
    return row >= 0 && row < pairCount() ? row : -1;
}

void PhotosPanel::pickPair(int row) {
    if (row >= 0 && row < pairs_->topLevelItemCount()) pairs_->setCurrentItem(pairs_->topLevelItem(row));
}

void PhotosPanel::setKeepFocal(bool keep) { keepFocal_->setChecked(keep); }

bool PhotosPanel::keepFocal() const { return keepFocal_->isChecked(); }

QString PhotosPanel::solveText() const { return solveText_; }

bool PhotosPanel::solve() {
    const int i = current();
    json all = photos();
    if (i < 0) {
        emit message(tr("Import a photo first."));
        return false;
    }
    const int width = all[i].value("width", 0), height = all[i].value("height", 0);
    const std::optional<RefCamera> was = camera(i);
    const ReferenceLandmarkSolve s =
        solveReferenceLandmarks(stateOf(all[i]).landmarks, width, height, was && was->focalPx > 0.0 ? was->focalPx : focalGuess(width),
                                keepFocal(), was ? &was->pose : nullptr, true);
    if (!s.ok) {
        solveText_ = qs(s.why);
        result_->setText(solveText_);
        emit message(qs(s.why));
        return false;
    }
    all[i]["camera"] = cameraJson(s.camera);
    all[i]["rms"] = s.rms;
    store(all);
    refreshPairs();
    emit displayChanged();  // (the camera moved: a view through it follows)
    return true;
}

void PhotosPanel::clearPairs() {
    const int i = current();
    json all = photos();
    if (i < 0) return;
    ReferencePhotoEditor::State state = stateOf(all[i]);
    state.landmarks = {};
    all[i]["state"] = json::parse(referencePhotoStateToJson(state));
    all[i].erase("rms");
    store(all);
    refreshPairs();
}

bool PhotosPanel::deletePair(int row) {
    const int i = current();
    json all = photos();
    if (i < 0 || row < 0 || row >= pairCount()) return false;
    ReferencePhotoEditor editor(stateOf(all[i]));
    const MarkerSide side = row < static_cast<int>(editor.landmarks().count(kLandmarkPhoto)) ? kLandmarkPhoto : kLandmarkMesh;
    editor.removeLandmark(side, static_cast<std::size_t>(row));
    all[i]["state"] = json::parse(referencePhotoStateToJson(editor.state()));
    store(all);
    refreshPairs();
    return true;
}

bool PhotosPanel::repickPoint(int row, bool photoSide) {
    const int i = current();
    json all = photos();
    if (i < 0 || row < 0 || row >= pairCount()) return false;
    ReferencePhotoEditor editor(stateOf(all[i]));
    const MarkerSide cleared = photoSide ? kLandmarkPhoto : kLandmarkMesh, kept = photoSide ? kLandmarkMesh : kLandmarkPhoto;
    const std::size_t at = static_cast<std::size_t>(row);
    if (at >= editor.landmarks().count(cleared)) return false;  // no point there to clear
    std::optional<MarkerPoint> keep;
    if (at < editor.landmarks().count(kept)) keep = editor.landmarks().point(kept, at);
    editor.removeLandmark(cleared, at);  // the pair goes; its other point waits at the end
    if (keep) editor.addLandmark(kept, *keep);
    all[i]["state"] = json::parse(referencePhotoStateToJson(editor.state()));
    store(all);
    refreshPairs();
    return true;
}

// ---- Looking through ----------------------------------------------------------------------------------

void PhotosPanel::setLookingThrough(const QString& name) {
    looking_ = name;
    refreshControls();
}

int PhotosPanel::lookingIndex() const {
    if (looking_.isEmpty()) return -1;
    for (int i = 0; i < count(); ++i) {
        if (name(i) == looking_) return i;
    }
    return -1;
}

std::set<std::string> PhotosPanel::menuState() const {
    std::set<std::string> state;
    if (document_) state.insert("groom");
    if (current() >= 0) state.insert("photo");
    return state;
}

QMenu* PhotosPanel::contextMenu(const QPoint& pos) {
    if (QListWidgetItem* item = list_->itemAt(pos)) list_->setCurrentItem(item);
    return listMenu(this, defs().get("lists/photos/context"), bindings_, options_, menuState());
}

QMenu* PhotosPanel::pairsMenu(const QPoint& pos) {
    if (QTreeWidgetItem* item = pairs_->itemAt(pos)) pairs_->setCurrentItem(item);
    if (pickedPair() < 0) return nullptr;
    return listMenu(this, defs().get("lists/photos/pairsContext"), bindings_, options_, menuState());
}

// ---- What it shows ------------------------------------------------------------------------------------

void PhotosPanel::refresh() {
    const json all = photos();
    const int was = list_->currentRow();
    updating_ = true;
    list_->clear();
    for (const json& p : all) {
        const QString path = qs(p.value("image", std::string()));
        const json shown = p.value("display", json::object());
        const bool locked = shown.is_object() && shown.value("locked", false);
        auto* item = new QListWidgetItem(QStringLiteral("%1\n%2 x %3%4")
                                             .arg(qs(p.value("name", std::string())))
                                             .arg(p.value("width", 0))
                                             .arg(p.value("height", 0))
                                             .arg(locked ? tr("  ·  locked") : QString()));
        item->setToolTip(path);
        const QImage image = picture(list_->count());
        if (!image.isNull()) item->setIcon(QIcon(QPixmap::fromImage(image.scaled(list_->iconSize() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        list_->addItem(item);
    }
    if (list_->count() > 0) list_->setCurrentRow(std::clamp(was, 0, list_->count() - 1));
    updating_ = false;
    bar_->refresh();
    pairsBar_->refresh();
    guidesBar_->refresh();
    refreshControls();
}

void PhotosPanel::refreshControls() {
    const int i = current();
    const bool on = i >= 0;
    for (QWidget* w : {static_cast<QWidget*>(look_), static_cast<QWidget*>(lock_), static_cast<QWidget*>(opacity_),
                       static_cast<QWidget*>(front_), static_cast<QWidget*>(behind_), static_cast<QWidget*>(visible_),
                       static_cast<QWidget*>(pick_), static_cast<QWidget*>(keepFocal_), static_cast<QWidget*>(pairs_)}) {
        w->setEnabled(on);
    }
    updating_ = true;
    const Display d = display(i);
    lock_->setChecked(on && d.locked);
    if (!opacity_->editing()) opacity_->setValue(d.opacity);
    focal_->setEnabled(on && !d.locked);  // (a locked camera keeps its focal length)
    if (const std::optional<RefCamera> c = on ? camera(i) : std::nullopt; c && c->focalPx > 0.0 && !focal_->editing()) {
        focal_->setValue(c->focalPx * 36.0 / std::max(imageSize(i).width(), 1));
    }
    front_->setChecked(d.front);
    behind_->setChecked(!d.front);
    visible_->setChecked(d.visible);
    const bool looking = on && !looking_.isEmpty() && name(i) == looking_;
    look_->setChecked(looking);
    look_->setText(looking ? tr("Back to Perspective") : tr("Look Through"));
    updating_ = false;
    bar_->refresh();
    pairsBar_->refresh();
    refreshPairs();
    // Where Guides from Photos stands: the photos, their landmarks and arrows, the next step.
    const json all = photos();
    if (all.empty()) {
        guidesHint_->setText(tr("Open Guides from Photos, import a photo of the groom and line it up with a few landmarks."));
    } else {
        QStringList rows;
        for (const json& p : all) {
            const ReferencePhotoEditor::State s = stateOf(p);
            const int pairs = static_cast<int>(s.landmarks.pairCount()), strokes = static_cast<int>(s.strokes.size());
            rows << tr("%1: %2 landmark pair%3, %4 flow arrow%5")
                        .arg(qs(p.value("name", std::string())))
                        .arg(pairs)
                        .arg(pairs == 1 ? QString() : QStringLiteral("s"))
                        .arg(strokes)
                        .arg(strokes == 1 ? QString() : QStringLiteral("s"));
        }
        guidesHint_->setText(rows.join(QStringLiteral("; ")) + tr(". Generate in Guides from Photos."));
    }
}

void PhotosPanel::refreshPairs() {
    const int i = current();
    const int picked = pairs_->indexOfTopLevelItem(pairs_->currentItem());
    pairs_->clear();
    if (i < 0) {
        alignAbout_->setText(tr("Import a photo to start."));
        result_->clear();
        return;
    }
    const json all = photos();
    const MarkerLists landmarks = stateOf(all[i]).landmarks;
    const int width = all[i].value("width", 0), height = all[i].value("height", 0);
    // Each complete pair's error through the camera the pairs give (the live solve, as the
    // Reference window shows it).
    const std::optional<RefCamera> was = camera(i);
    const std::size_t need = keepFocal() ? kReferenceMinPairs : kReferenceMinPairsFocal;
    ReferenceLandmarkSolve s;
    if (landmarks.pairCount() >= kReferenceMinPairs) {
        s = solveReferenceLandmarks(landmarks, width, height, was && was->focalPx > 0.0 ? was->focalPx : focalGuess(width), keepFocal(),
                                    was ? &was->pose : nullptr, false);
    }
    const std::size_t rows = std::max(landmarks.count(kLandmarkPhoto), landmarks.count(kLandmarkMesh));
    for (std::size_t k = 0; k < rows; ++k) {
        const QString image = k < landmarks.count(kLandmarkPhoto)
                                  ? QStringLiteral("%1, %2").arg(landmarks.point(kLandmarkPhoto, k).point.x, 0, 'f', 0).arg(landmarks.point(kLandmarkPhoto, k).point.y, 0, 'f', 0)
                                  : tr("pick on the photo");
        const QString mesh = k < landmarks.count(kLandmarkMesh)
                                 ? QStringLiteral("%1, %2, %3")
                                       .arg(landmarks.point(kLandmarkMesh, k).point.x, 0, 'f', 2)
                                       .arg(landmarks.point(kLandmarkMesh, k).point.y, 0, 'f', 2)
                                       .arg(landmarks.point(kLandmarkMesh, k).point.z, 0, 'f', 2)
                                 : tr("pick on the mesh");
        const QString error = s.ok && k < s.errors.size() ? QStringLiteral("%1 px").arg(s.errors[k], 0, 'f', 1) : QString();
        auto* item = new QTreeWidgetItem(pairs_, {QString::number(k + 1), image, mesh, error});
        item->setToolTip(1, image);
        item->setToolTip(2, mesh);
        if (s.ok && k < s.bad.size() && s.bad[k]) item->setForeground(3, theme::errorText());
    }
    for (int column : {0, 1, 3}) pairs_->resizeColumnToContents(column);
    if (picked >= 0 && picked < pairs_->topLevelItemCount()) pairs_->setCurrentItem(pairs_->topLevelItem(picked));
    alignAbout_->setText(tr("Click a feature on the photo, then the same feature on the mesh (Pick Points: side by side in Guides from Photos). "
                            "%1 of %2 pairs%3.")
                             .arg(landmarks.pairCount())
                             .arg(need)
                             .arg(keepFocal() ? tr(" (focal length kept)") : tr(", or 4 keeping the focal length")));
    const double rms = all[i].value("rms", -1.0);
    solveText_ = rms >= 0.0 && was ? tr("Last solve: %1 px off on average, focal length %2 mm.")
                                         .arg(rms, 0, 'f', 2)
                                         .arg(was->focalPx * 36.0 / std::max(width, 1), 0, 'f', 1)
                                   : QString();
    result_->setText(solveText_);
}

}  // namespace groomlab::app
