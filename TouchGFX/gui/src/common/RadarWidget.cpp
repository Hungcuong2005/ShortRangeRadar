#include <gui/common/RadarWidget.hpp>
#include <touchgfx/Color.hpp>

// LUT lượng giác từ 0 đến 180 độ, scale = 1024
static const int16_t sinLut[181] = {
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213, 230, 248, 265, 282, 299, 316, 333, 350, 367, 384, 400, 416, 433, 463, 481, 496, 512, 527, 543, 558, 573, 587, 602, 616, 630, 644, 658, 672, 685, 698, 711, 724, 737, 749, 761, 773, 784, 796, 807, 818, 828, 839, 849, 859, 868, 878, 887, 896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989, 994, 998, 1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023, 1024, 1024, 1024, 1023, 1023, 1022, 1020, 1018, 1016, 1014, 1011, 1008, 1005, 1002, 998, 994, 989, 984, 979, 974, 968, 962, 956, 949, 943, 935, 928, 920, 912, 904, 896, 887, 878, 868, 859, 849, 839, 828, 818, 807, 796, 784, 773, 761, 749, 737, 724, 711, 698, 685, 672, 658, 644, 630, 616, 602, 587, 573, 558, 543, 527, 512, 496, 481, 465, 449, 433, 416, 400, 384, 367, 350, 333, 316, 299, 282, 265, 248, 230, 213, 195, 178, 160, 143, 125, 107, 89, 71, 54, 36, 18, 0
};

static const int16_t cosLut[181] = {
    1024, 1024, 1023, 1023, 1022, 1020, 1018, 1016, 1014, 1011, 1008, 1005, 1002, 998, 994, 989, 984, 979, 974, 968, 962, 956, 949, 943, 935, 928, 920, 912, 904, 896, 887, 878, 868, 859, 849, 839, 828, 818, 807, 796, 784, 773, 761, 749, 737, 724, 711, 698, 685, 672, 658, 644, 630, 616, 602, 587, 573, 558, 543, 527, 512, 496, 481, 465, 449, 433, 416, 400, 384, 367, 350, 333, 316, 299, 282, 265, 248, 230, 213, 195, 178, 160, 143, 125, 107, 89, 71, 54, 36, 18, 0, -18, -36, -54, -71, -89, -107, -125, -143, -160, -178, -195, -213, -230, -248, -265, -282, -299, -316, -333, -350, -367, -384, -400, -416, -433, -449, -465, -481, -496, -512, -527, -543, -558, -573, -587, -602, -616, -630, -644, -658, -672, -685, -698, -711, -724, -737, -749, -761, -773, -784, -796, -807, -818, -828, -839, -849, -859, -868, -878, -887, -896, -904, -912, -920, -928, -935, -943, -949, -956, -962, -968, -974, -979, -984, -989, -994, -998, -1002, -1005, -1008, -1011, -1014, -1016, -1018, -1020, -1022, -1023, -1023, -1024, -1024
};

static inline int16_t abs_diff(int16_t a, int16_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

RadarWidget::RadarWidget() :
    sweepHistoryCount(0),
    lastSampleAngle(90),
    lastSampleDistance(0),
    lastSampleValid(0)
{
    // Cấu hình kích thước của RadarWidget
    Container::setWidth(240);
    Container::setHeight(180);
    Container::setTouchable(false);

    // 1. Nền đen
    background.setPosition(0, 0, 240, 180);
    background.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0));
    background.setTouchable(false);
    add(background);

    // Cấu hình Painters cho lưới (Dark Green)
    gridPainter.setColor(touchgfx::Color::getColorFromRGB(0, 100, 0));

    // Khởi tạo các màu cho đuôi tia quét (sweepPainters)
    sweepPainters[0].setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));
    sweepPainters[1].setColor(touchgfx::Color::getColorFromRGB(0, 190, 0));
    sweepPainters[2].setColor(touchgfx::Color::getColorFromRGB(0, 128, 0));
    sweepPainters[3].setColor(touchgfx::Color::getColorFromRGB(0, 75, 0));
    sweepPainters[4].setColor(touchgfx::Color::getColorFromRGB(0, 35, 0));

    // 2. Container clipping cho lưới bán nguyệt cố định (chỉ lấy nửa trên)
    gridClippingContainer.setPosition(0, 0, 240, 170);
    gridClippingContainer.setTouchable(false);
    add(gridClippingContainer);

    // Bốn vòng lưới tương ứng 25, 50, 75, 100 cm
    // Lưới 25cm (R = 26px)
    gridCircle1.setPosition(0, 0, 240, 170);
    gridCircle1.setCenter(CENTER_X, CENTER_Y);
    gridCircle1.setRadius(26);
    gridCircle1.setLineWidth(1);
    gridCircle1.setPainter(gridPainter);
    gridCircle1.setTouchable(false);
    gridClippingContainer.add(gridCircle1);

    // Lưới 50cm (R = 53px)
    gridCircle2.setPosition(0, 0, 240, 170);
    gridCircle2.setCenter(CENTER_X, CENTER_Y);
    gridCircle2.setRadius(53);
    gridCircle2.setLineWidth(1);
    gridCircle2.setPainter(gridPainter);
    gridCircle2.setTouchable(false);
    gridClippingContainer.add(gridCircle2);

    // Lưới 75cm (R = 79px)
    gridCircle3.setPosition(0, 0, 240, 170);
    gridCircle3.setCenter(CENTER_X, CENTER_Y);
    gridCircle3.setRadius(79);
    gridCircle3.setLineWidth(1);
    gridCircle3.setPainter(gridPainter);
    gridCircle3.setTouchable(false);
    gridClippingContainer.add(gridCircle3);

    // Lưới 100cm (R = 105px)
    gridCircle4.setPosition(0, 0, 240, 170);
    gridCircle4.setCenter(CENTER_X, CENTER_Y);
    gridCircle4.setRadius(RADAR_RADIUS);
    gridCircle4.setLineWidth(1);
    gridCircle4.setPainter(gridPainter);
    gridCircle4.setTouchable(false);
    gridClippingContainer.add(gridCircle4);

    // Các đường hướng góc 45°, 90°, 135°
    auto initGridLine = [this](touchgfx::Line& line, uint16_t angle) {
        line.setPosition(0, 0, 240, 170);
        line.setLineWidth(1);
        line.setPainter(gridPainter);
        line.setStart(CENTER_X, CENTER_Y);
        int16_t endX, endY;
        getCoordinates(angle, RADAR_RADIUS, endX, endY);
        line.setEnd(endX, endY);
        line.setTouchable(false);
        gridClippingContainer.add(line);
    };

    initGridLine(gridLine45, 45);
    initGridLine(gridLine90, 90);
    initGridLine(gridLine135, 135);

    // Đường cơ sở (Baseline) ở y = 170
    baseline.setPosition(0, CENTER_Y, 240, 1);
    baseline.setColor(touchgfx::Color::getColorFromRGB(0, 100, 0));
    baseline.setTouchable(false);
    add(baseline);

    // 3. Khởi tạo nhãn tĩnh khoảng cách và góc (sử dụng font Small)
    // Các nhãn được add trực tiếp vào RadarWidget để tránh bị container lưới cắt mất chữ
    auto initLabel = [this](touchgfx::TextArea& label, touchgfx::TypedTextId textId, int16_t x, int16_t y, int16_t w, int16_t h) {
        label.setPosition(x, y, w, h);
        label.setColor(touchgfx::Color::getColorFromRGB(0, 100, 0)); // Màu xanh tối giống lưới
        label.setTypedText(touchgfx::TypedText(textId));
        label.setTouchable(false);
        add(label);
    };

    // Nhãn khoảng cách đặt lệch nhẹ sang trái của đường 90 độ (đường 90 độ nằm ở x = 120)
    initLabel(labelRange25, T_RADAR_RANGE_25, 105, 139, 15, 10);
    initLabel(labelRange50, T_RADAR_RANGE_50, 105, 112, 15, 10);
    initLabel(labelRange75, T_RADAR_RANGE_75, 105, 86, 15, 10);
    initLabel(labelRange100, T_RADAR_RANGE_100, 98, 60, 20, 10);
    initLabel(labelUnitCm, T_RADAR_UNIT_CM, 122, 60, 15, 10); // chữ "cm" đặt lệch sang phải

    // Nhãn góc đặt gần biên ngoài tương ứng các đường quét
    initLabel(labelAngle0, T_RADAR_ANGLE_0, 222, 158, 18, 10);
    initLabel(labelAngle45, T_RADAR_ANGLE_45, 198, 86, 20, 10);
    initLabel(labelAngle90, T_RADAR_ANGLE_90, 108, 48, 25, 10);
    initLabel(labelAngle135, T_RADAR_ANGLE_135, 20, 86, 25, 10);
    initLabel(labelAngle180, T_RADAR_ANGLE_180, 0, 158, 25, 10);

    // 4. Khởi tạo 5 đường tia quét (sweepLines)
    for (uint8_t i = 0; i < SWEEP_TRAIL_COUNT; ++i)
    {
        sweepAngleHistory[i] = 90;
        
        sweepLines[i].setPosition(0, 0, 240, 180);
        sweepLines[i].setLineWidth(2);
        sweepLines[i].setPainter(sweepPainters[i]);
        sweepLines[i].setStart(CENTER_X, CENTER_Y);
        sweepLines[i].setEnd(CENTER_X, CENTER_Y);
        sweepLines[i].setTouchable(false);
        sweepLines[i].setVisible(false);
        add(sweepLines[i]);
    }

    // 5. Khởi tạo 37 chấm đỏ vật cản (detectionBins)
    for (uint8_t i = 0; i < DETECTION_BIN_COUNT; ++i)
    {
        detectionBins[i].distanceMm = 0;
        detectionBins[i].ageSamples = 0;
        detectionBins[i].active = false;

        detectionDots[i].setPosition(0, 0, 6, 6);
        detectionDots[i].setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
        detectionDots[i].setTouchable(false);
        detectionDots[i].setVisible(false);
        add(detectionDots[i]);
    }
}

RadarWidget::~RadarWidget()
{
}

void RadarWidget::getCoordinates(uint16_t angle, int16_t radius, int16_t& outX, int16_t& outY) const
{
    if (angle > 180)
    {
        angle = 180;
    }

    // Phép biến đổi lượng giác fixed-point
    outX = CENTER_X + (radius * cosLut[angle]) / 1024;
    outY = CENTER_Y - (radius * sinLut[angle]) / 1024;
}

void RadarWidget::setSample(uint16_t angle, uint16_t distanceMm, uint8_t valid)
{
    if (angle == lastSampleAngle && distanceMm == lastSampleDistance && valid == lastSampleValid)
    {
        return;
    }

    // 1. Clamp angle trước khi tính index
    uint16_t clampedAngle = angle;
    if (clampedAngle < DETECTION_MIN_ANGLE)
    {
        clampedAngle = DETECTION_MIN_ANGLE;
    }
    else if (clampedAngle > DETECTION_MAX_ANGLE)
    {
        clampedAngle = DETECTION_MAX_ANGLE;
    }

    // Tính chỉ số mảng bin
    uint16_t currentBinIndex = (clampedAngle - DETECTION_MIN_ANGLE + DETECTION_ANGLE_STEP / 2U) / DETECTION_ANGLE_STEP;
    if (currentBinIndex >= DETECTION_BIN_COUNT)
    {
        currentBinIndex = DETECTION_BIN_COUNT - 1U;
    }

    // 2. Tăng tuổi (aging) tất cả các bin đang active
    for (uint16_t i = 0; i < DETECTION_BIN_COUNT; ++i)
    {
        if (detectionBins[i].active)
        {
            if (detectionBins[i].ageSamples < UINT16_MAX)
            {
                ++detectionBins[i].ageSamples;
            }

            // 3. Áp dụng fail-safe timeout
            if (detectionBins[i].ageSamples > DETECTION_MAX_AGE)
            {
                detectionBins[i].active = false;
            }
        }
    }

    // 4 & 5. Cập nhật dòng current bin bằng raw sample
    if (valid != 0U && distanceMm >= 20U && distanceMm <= RADAR_MAX_DISTANCE_MM)
    {
        detectionBins[currentBinIndex].active = true;
        detectionBins[currentBinIndex].distanceMm = distanceMm;
        detectionBins[currentBinIndex].ageSamples = 0U;
    }
    else
    {
        detectionBins[currentBinIndex].active = false;
        detectionBins[currentBinIndex].distanceMm = 0U;
        detectionBins[currentBinIndex].ageSamples = 0U;
    }

    // 6. Cập nhật Đuôi tia quét (Sweep Trail) với logic tương đương clamp gốc
    if (sweepHistoryCount == 0 || abs_diff(clampedAngle, sweepAngleHistory[0]) >= 1)
    {
        for (int i = SWEEP_TRAIL_COUNT - 1; i > 0; --i)
        {
            sweepAngleHistory[i] = sweepAngleHistory[i - 1];
        }
        sweepAngleHistory[0] = clampedAngle;

        if (sweepHistoryCount < SWEEP_TRAIL_COUNT)
        {
            sweepHistoryCount++;
        }

        for (uint8_t i = 0; i < sweepHistoryCount; ++i)
        {
            int16_t endX, endY;
            getCoordinates(sweepAngleHistory[i], RADAR_RADIUS, endX, endY);
            sweepLines[i].setEnd(endX, endY);
            sweepLines[i].setVisible(true);
        }
    }

    // 7. Đồng bộ hóa hiển thị 37 chấm đỏ và gọi Invalidate
    updateDetectionDots();
    invalidate();

    lastSampleAngle = angle;
    lastSampleDistance = distanceMm;
    lastSampleValid = valid;
}

void RadarWidget::updateDetectionDots()
{
    for (uint16_t i = 0; i < DETECTION_BIN_COUNT; ++i)
    {
        if (detectionBins[i].active)
        {
            uint16_t binAngle = DETECTION_MIN_ANGLE + i * DETECTION_ANGLE_STEP;
            int16_t dotRadius = (detectionBins[i].distanceMm * RADAR_RADIUS) / RADAR_MAX_DISTANCE_MM;
            int16_t targetX, targetY;
            getCoordinates(binAngle, dotRadius, targetX, targetY);

            int16_t dotX = targetX - 3;
            int16_t dotY = targetY - 3;

            if (dotX < 0) dotX = 0;
            if (dotX > 234) dotX = 234;
            if (dotY < 0) dotY = 0;
            if (dotY > 174) dotY = 174;

            detectionDots[i].setXY(dotX, dotY);

            uint16_t age = detectionBins[i].ageSamples;
            uint8_t red = 0;
            
            if (age <= 8)
            {
                red = 255;
            }
            else if (age <= 24)
            {
                red = 170;
            }
            else if (age <= 48)
            {
                red = 100;
            }
            else if (age <= 72)
            {
                red = 50;
            }

            detectionDots[i].setColor(touchgfx::Color::getColorFromRGB(red, 0, 0));
            detectionDots[i].setVisible(true);
        }
        else
        {
            detectionDots[i].setVisible(false);
        }
    }
}
