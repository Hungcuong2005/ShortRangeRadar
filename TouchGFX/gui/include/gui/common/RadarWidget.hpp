#ifndef RADARWIDGET_HPP
#define RADARWIDGET_HPP

#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include <touchgfx/widgets/canvas/Line.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class RadarWidget : public touchgfx::Container
{
public:
    RadarWidget();
    virtual ~RadarWidget();

    // Cập nhật giá trị mẫu đo radar
    void setSample(uint16_t angle, uint16_t distanceMm, uint8_t valid);

private:
    static const uint16_t RADAR_MAX_DISTANCE_MM = 1000;
    static const int16_t CENTER_X = 120;
    static const int16_t CENTER_Y = 170;
    static const int16_t RADAR_RADIUS = 105;

    static const uint8_t SWEEP_TRAIL_COUNT = 5;

    // Cấu hình Angle-Bin (Phải đồng bộ với radar_app.h)
    static constexpr uint16_t DETECTION_MIN_ANGLE = 0U;
    static constexpr uint16_t DETECTION_MAX_ANGLE = 180U;
    static constexpr uint16_t DETECTION_ANGLE_STEP = 5U;
    
    static constexpr uint16_t DETECTION_BIN_COUNT = 
        ((DETECTION_MAX_ANGLE - DETECTION_MIN_ANGLE) / DETECTION_ANGLE_STEP) + 1U;
        
    static constexpr uint16_t DETECTION_MAX_AGE = 
        2U * (DETECTION_BIN_COUNT - 1U); // 72 samples

    static_assert(DETECTION_ANGLE_STEP > 0U,
                  "Detection angle step must be non-zero");
    static_assert(DETECTION_MAX_ANGLE >= DETECTION_MIN_ANGLE,
                  "Invalid detection angle range");
    static_assert(
        ((DETECTION_MAX_ANGLE - DETECTION_MIN_ANGLE) % DETECTION_ANGLE_STEP) == 0U,
        "Detection range must be divisible by step");
    static_assert(DETECTION_BIN_COUNT == 37U,
                  "0..180 degrees with 5-degree step must have 37 bins");

    struct DetectionBin
    {
        uint16_t distanceMm;
        uint16_t ageSamples;
        bool active;
    };

    // Container dùng để clip các đường tròn lưới (chỉ lấy nửa trên)
    touchgfx::Container gridClippingContainer;

    // Thành phần tĩnh (Lưới)
    touchgfx::Box background;
    touchgfx::Box baseline;
    
    // Bốn vòng lưới tương ứng 25, 50, 75, 100 cm
    touchgfx::Circle gridCircle1;
    touchgfx::Circle gridCircle2;
    touchgfx::Circle gridCircle3;
    touchgfx::Circle gridCircle4;
    
    touchgfx::Line gridLine45;
    touchgfx::Line gridLine90;
    touchgfx::Line gridLine135;

    // Các nhãn tĩnh khoảng cách và góc (Verdana 10)
    touchgfx::TextArea labelRange25;
    touchgfx::TextArea labelRange50;
    touchgfx::TextArea labelRange75;
    touchgfx::TextArea labelRange100;
    touchgfx::TextArea labelUnitCm;
    touchgfx::TextArea labelAngle0;
    touchgfx::TextArea labelAngle45;
    touchgfx::TextArea labelAngle90;
    touchgfx::TextArea labelAngle135;
    touchgfx::TextArea labelAngle180;

    // Thành phần động: 5 đường tia quét và mảng 37 chấm vật cản
    touchgfx::Line sweepLines[SWEEP_TRAIL_COUNT];
    touchgfx::Box detectionDots[DETECTION_BIN_COUNT];

    // Painters có vòng đời an toàn (là member của class)
    touchgfx::PainterRGB565 gridPainter;
    touchgfx::PainterRGB565 sweepPainters[SWEEP_TRAIL_COUNT];

    // Lịch sử tia quét
    int16_t sweepAngleHistory[SWEEP_TRAIL_COUNT];
    uint8_t sweepHistoryCount;

    // Các bin chứa lịch sử vật cản
    DetectionBin detectionBins[DETECTION_BIN_COUNT];

    // Trạng thái của mẫu đo liền trước để tránh invalidate thừa
    uint16_t lastSampleAngle;
    uint16_t lastSampleDistance;
    uint8_t lastSampleValid;

    // Hàm tiện ích tính toán tọa độ
    void getCoordinates(uint16_t angle, int16_t radius, int16_t& outX, int16_t& outY) const;
    
    // Cập nhật hiển thị và màu sắc của các chấm vật cản dựa trên tuổi
    void updateDetectionDots();
};

#endif // RADARWIDGET_HPP
