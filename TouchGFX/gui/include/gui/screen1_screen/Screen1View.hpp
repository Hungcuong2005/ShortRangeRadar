#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <gui/common/RadarWidget.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/TextArea.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
protected:
    static const uint16_t TXTANGLE_SIZE = 20;
    touchgfx::Unicode::UnicodeChar txtAngleBuffer[TXTANGLE_SIZE];

    static const uint16_t TXTDISTANCE_SIZE = 20;
    touchgfx::Unicode::UnicodeChar txtDistanceBuffer[TXTDISTANCE_SIZE];

    static const uint16_t TXTSTATUS_SIZE = 20;
    touchgfx::Unicode::UnicodeChar txtStatusBuffer[TXTSTATUS_SIZE];

    // Đối tượng widget Radar vẽ cung bán nguyệt và vật cản
    RadarWidget radarWidget;

    // Đèn LED chỉ thị trạng thái TARGET/SCANNING
    touchgfx::Box statusLed;

    static constexpr uint8_t TARGET_HOLD_FAST_SAMPLES = 5U;
    static constexpr uint8_t TARGET_HOLD_SLOW_SAMPLES = 3U;
    uint8_t targetHoldCounter;
    bool displayedTargetState;

    static constexpr uint8_t DISTANCE_HOLD_FAST_SAMPLES = 3U;
    static constexpr uint8_t DISTANCE_HOLD_SLOW_SAMPLES = 2U;
    uint16_t lastValidDistanceMm;
    uint8_t distanceHoldCounter;
    bool hasLastValidDistance;

    uint8_t getTargetHoldSamples(uint16_t speed) const;
    uint8_t getDistanceHoldSamples(uint16_t speed) const;

    // UI State Cache variables
    uint16_t lastRawAngle;
    bool lastDisplayedTargetState;
    bool lastHasValidDistance;
    uint16_t lastDisplayedDistance;
    
    // UI Speed Overlay
    touchgfx::Container speedSettingsOverlay;
    touchgfx::Box speedSettingsBackground;
    touchgfx::TextArea speedTitle;
    touchgfx::TextArea radarPausedText;
    touchgfx::TextArea currentSpeedText;
    touchgfx::TextArea selectSpeedText;
    
    uint32_t lastControlState;
    uint16_t lastSpeedMode;
};

#endif // SCREEN1VIEW_HPP
