#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/canvas_widget_renderer/CanvasWidgetRenderer.hpp>

extern "C" {
#include "radar_app.h"
}
#include <texts/TextKeysAndLanguages.hpp>

// Chế độ mô phỏng kiểm thử giao diện Radar
// Đặt bằng 1 để tự động chạy chu kỳ kiểm thử trong Simulator
#define RADAR_WIDGET_TEST_MODE 0

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    
    // Cấp phát tĩnh buffer 12KB cho CanvasWidgetRenderer (CWR)
    static const uint32_t CWR_BUFFER_SIZE = 12288;
    static uint8_t cwrBuffer[CWR_BUFFER_SIZE];
    static bool cwrBufferInitialized = false;
    if (!cwrBufferInitialized)
    {
        touchgfx::CanvasWidgetRenderer::setupBuffer(cwrBuffer, CWR_BUFFER_SIZE);
        cwrBufferInitialized = true;
    }

    // Thiết lập vị trí của radarWidget
    radarWidget.setXY(0, 140);
    add(radarWidget);

    // Gán wildcard buffers thủ công
    txtAngle.setWildcard(txtAngleBuffer);
    txtDistance.setWildcard(txtDistanceBuffer);
    txtStatus.setWildcard(txtStatusBuffer);
    
    // Sắp xếp lại bố cục phần đầu màn hình (y = 0 đến 140)
    // Căn giữa tiêu đề chính "Radar Project"
    textArea1.setPosition(52, 10, 136, 24);
    
    // Đặt vị trí hiển thị góc và khoảng cách
    txtAngle.setPosition(20, 40, 200, 30);
    txtDistance.setPosition(20, 70, 220, 30);
    
    // Dịch chuyển txtStatus sang phải một chút để chừa chỗ cho đèn LED
    txtStatus.setPosition(45, 105, 180, 25);
    
    // Đèn LED trạng thái đặt tại (20, 112, 10, 10)
    statusLed.setPosition(20, 112, 10, 10);
    statusLed.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0)); // Xanh lá mặc định (SCANNING)
    statusLed.setTouchable(false);
    add(statusLed);
    
    // Đảm bảo chữ hiển thị màu trắng rõ ràng trên nền tối
    textArea1.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtAngle.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtDistance.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtStatus.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    
    // Thiết lập giá trị ban đầu cho các text (phù hợp với baseline và font)
    Unicode::snprintf(txtAngleBuffer, TXTANGLE_SIZE, "%d deg", 90);
    Unicode::strncpy(txtDistanceBuffer, "-- cm", TXTDISTANCE_SIZE);
    Unicode::strncpy(txtStatusBuffer, "SCANNING", TXTSTATUS_SIZE);
    
    targetHoldCounter = 0U;
    displayedTargetState = false;

    distanceHoldCounter = 0U;
    lastValidDistanceMm = 0U;
    hasLastValidDistance = false;
    
    lastRawAngle = 999;
    lastDisplayedTargetState = false;
    lastHasValidDistance = false;
    lastDisplayedDistance = 9999;
    
    txtDistance.invalidate();
    txtStatus.invalidate();
    statusLed.invalidate();
    
    speedSettingsOverlay.setPosition(0, 0, 240, 320);
    speedSettingsBackground.setPosition(0, 0, 240, 320);
    speedSettingsBackground.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0));
    speedSettingsOverlay.add(speedSettingsBackground);

    speedTitle.setTypedText(touchgfx::TypedText(T_T_SPEED_TITLE));
    speedTitle.setPosition(0, 50, 240, 50);
    speedTitle.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    speedSettingsOverlay.add(speedTitle);

    radarPausedText.setTypedText(touchgfx::TypedText(T_T_SPEED_RADAR_PAUSED));
    radarPausedText.setPosition(0, 120, 240, 30);
    radarPausedText.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
    speedSettingsOverlay.add(radarPausedText);

    currentSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_CURRENT_FAST));
    currentSpeedText.setPosition(0, 160, 240, 30);
    currentSpeedText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    speedSettingsOverlay.add(currentSpeedText);

    selectSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_SELECT_SLOW));
    selectSpeedText.setPosition(0, 210, 240, 30);
    selectSpeedText.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));
    speedSettingsOverlay.add(selectSpeedText);

    speedSettingsOverlay.setVisible(false);
    add(speedSettingsOverlay);

    lastControlState = g_radarControlState.packed;
    RadarControlState initialState;
    initialState.packed = lastControlState;
    lastSpeedMode = initialState.state.speedMode;
}

uint8_t Screen1View::getTargetHoldSamples(uint16_t speed) const
{
    return (speed == RADAR_SPEED_SLOW) ? TARGET_HOLD_SLOW_SAMPLES : TARGET_HOLD_FAST_SAMPLES;
}

uint8_t Screen1View::getDistanceHoldSamples(uint16_t speed) const
{
    return (speed == RADAR_SPEED_SLOW) ? DISTANCE_HOLD_SLOW_SAMPLES : DISTANCE_HOLD_FAST_SAMPLES;
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::handleTickEvent()
{
#if RADAR_WIDGET_TEST_MODE
    static uint32_t testState = 0;
    static uint32_t tickCount = 0;
    tickCount++;
    if (tickCount % 60 == 0) // Cập nhật mỗi giây (60 ticks)
    {
        uint16_t angle = 90;
        uint16_t dist = 500;
        uint8_t valid = 1;
        
        switch (testState)
        {
            case 0: // 0 deg, 500 mm (50 cm) -> TARGET
                angle = 0; dist = 500; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0)); // Đỏ
                break;
            case 1: // 90 deg, 500 mm (50 cm) -> TARGET
                angle = 90; dist = 500; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
                break;
            case 2: // 180 deg, 500 mm (50 cm) -> TARGET
                angle = 180; dist = 500; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
                break;
            case 3: // 90 deg, 100 mm (10 cm) -> TARGET
                angle = 90; dist = 100; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
                break;
            case 4: // 90 deg, 1000 mm (100 cm) -> TARGET
                angle = 90; dist = 1000; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
                break;
            case 5: // 90 deg, invalid -> SCANNING (Chấm đỏ phai màu và biến mất)
                angle = 90; dist = 500; valid = 0;
                Unicode::strncpy(txtStatusBuffer, "SCANNING", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0)); // Xanh lá
                break;
            case 6: // 90 deg, 1200 mm (>1000) -> SCANNING
                angle = 90; dist = 1200; valid = 1;
                Unicode::strncpy(txtStatusBuffer, "SCANNING", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));
                break;
        }
        
        radarWidget.setSample(angle, dist, valid);
        
        Unicode::snprintf(txtAngleBuffer, TXTANGLE_SIZE, "%d deg", angle);
        txtAngle.invalidate();
        
        if (valid && (dist >= 20) && (dist <= 1000))
        {
            Unicode::snprintf(txtDistanceBuffer, TXTDISTANCE_SIZE, "%d cm", dist / 10);
        }
        else
        {
            Unicode::strncpy(txtDistanceBuffer, "-- cm", TXTDISTANCE_SIZE);
        }
        
        txtDistance.invalidate();
        txtStatus.invalidate();
        statusLed.invalidate();
        
        testState = (testState + 1) % 7;
    }
#else
    uint32_t currentControlState = g_radarControlState.packed;
    if (currentControlState != lastControlState)
    {
        RadarControlState state;
        state.packed = currentControlState;
        
        if (state.state.speedMode != lastSpeedMode)
        {
            uint8_t newTargetMax = getTargetHoldSamples(state.state.speedMode);
            uint8_t newDistanceMax = getDistanceHoldSamples(state.state.speedMode);
            if (targetHoldCounter > newTargetMax)
            {
                targetHoldCounter = newTargetMax;
            }
            if (distanceHoldCounter > newDistanceMax)
            {
                distanceHoldCounter = newDistanceMax;
            }
            lastSpeedMode = state.state.speedMode;
        }
        
        if (state.state.appMode == RADAR_APP_SPEED_SETTING)
        {
            if (state.state.speedMode == RADAR_SPEED_FAST)
            {
                currentSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_CURRENT_FAST));
                selectSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_SELECT_SLOW));
            }
            else
            {
                currentSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_CURRENT_SLOW));
                selectSpeedText.setTypedText(touchgfx::TypedText(T_T_SPEED_SELECT_FAST));
            }
            speedSettingsOverlay.setVisible(true);
            speedSettingsOverlay.invalidate();
        }
        else
        {
            speedSettingsOverlay.setVisible(false);
            speedSettingsOverlay.invalidate(); // Invalidate the whole area to redraw what was underneath
        }
        
        lastControlState = currentControlState;
    }

    if (radarQueueHandle == NULL)
    {
        return;
    }

    RadarSample latest;
    RadarSample temp;
    bool hasSample = false;

    // Lấy giá trị mẫu mới nhất ra khỏi Queue
    while (osMessageQueueGet(radarQueueHandle, &temp, NULL, 0U) == osOK)
    {
        latest = temp;
        hasSample = true;
    }

    if (hasSample)
    {
        // 1. Cập nhật Text Góc (chỉ format và invalidate nếu đổi góc)
        if (latest.angle_deg != lastRawAngle)
        {
            Unicode::snprintf(txtAngleBuffer, TXTANGLE_SIZE, "%d deg", latest.angle_deg);
            txtAngle.invalidate();
            lastRawAngle = latest.angle_deg;
        }

        // Kiểm tra xem mẫu đo có hợp lệ làm TARGET hay không
        bool isTargetValid = (latest.valid != 0U && latest.distance_mm >= 20U && latest.distance_mm <= 1000U);

        // 2. Logic TARGET hold (5 samples)
        if (isTargetValid)
        {
            targetHoldCounter = getTargetHoldSamples(lastSpeedMode);
            displayedTargetState = true;
        }
        else
        {
            if (targetHoldCounter > 0U)
            {
                targetHoldCounter--;
            }
            if (targetHoldCounter == 0U)
            {
                displayedTargetState = false;
            }
        }

        // 3. Logic Distance hold (3 samples)
        if (isTargetValid)
        {
            lastValidDistanceMm = latest.distance_mm;
            distanceHoldCounter = getDistanceHoldSamples(lastSpeedMode);
            hasLastValidDistance = true;
        }
        else
        {
            if (distanceHoldCounter > 0U)
            {
                distanceHoldCounter--;
            }
            if (distanceHoldCounter == 0U)
            {
                hasLastValidDistance = false;
            }
        }

        // 4. Cập nhật UI Status và LED dựa trên Target Hold
        if (displayedTargetState != lastDisplayedTargetState)
        {
            if (displayedTargetState)
            {
                Unicode::strncpy(txtStatusBuffer, "TARGET", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));
            }
            else
            {
                Unicode::strncpy(txtStatusBuffer, "SCANNING", TXTSTATUS_SIZE);
                statusLed.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));
            }
            txtStatus.invalidate();
            statusLed.invalidate();
            lastDisplayedTargetState = displayedTargetState;
        }

        // 5. Cập nhật UI Distance dựa trên Distance Hold
        if (hasLastValidDistance)
        {
            if (!lastHasValidDistance || lastDisplayedDistance != lastValidDistanceMm)
            {
                Unicode::snprintf(txtDistanceBuffer, TXTDISTANCE_SIZE, "%d cm", lastValidDistanceMm / 10);
                txtDistance.invalidate();
                lastDisplayedDistance = lastValidDistanceMm;
            }
        }
        else
        {
            if (lastHasValidDistance)
            {
                Unicode::strncpy(txtDistanceBuffer, "-- cm", TXTDISTANCE_SIZE);
                txtDistance.invalidate();
            }
        }
        lastHasValidDistance = hasLastValidDistance;

        // 6. Truyền raw sample nguyên bản xuống cho RadarWidget xử lý history
        radarWidget.setSample(latest.angle_deg, latest.distance_mm, latest.valid);
    }
#endif
}
