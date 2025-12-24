 #include "status_display.h"
 
 // Pattern definitions -------------------------------------------------
 
 // Ready/handshake patterns (LED 0)
 static constexpr StatusDisplay::BlinkStep PATTERN_BOOT_STEPS[] = {
     {120, true}, {380, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_IDLE_STEPS[] = {
     {120, true}, {880, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_DETECT_STEPS[] = {
     {120, true}, {120, false}, {120, true}, {640, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_NEGOTIATE_STEPS[] = {
     {150, true}, {150, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_WAIT_ACK_STEPS[] = {
     {80, true}, {120, false}, {80, true}, {720, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_EXCHANGE_STEPS[] = {
     {220, true}, {220, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_SUCCESS_STEPS[] = {
     {500, true}, {500, false}
 };
 static constexpr StatusDisplay::BlinkStep PATTERN_ERROR_STEPS[] = {
     {80, true}, {80, false}, {80, true}, {80, false}, {80, true}, {500, false}
 };
 
 // Role patterns (LED 1)
 static constexpr StatusDisplay::BlinkStep PATTERN_ROLE_UNKNOWN_STEPS[] = {
     {90, true}, {910, false}
 };
 
 // --------------------------------------------------------------------
 
 StatusDisplay::StatusDisplay()
     : _pins{0}
     , _ledCount(0)
     , _states{}
     , _initialized(false) {
 }
 
 void StatusDisplay::begin(const uint32_t* ledPins, size_t ledCount) {
     if (!ledPins || ledCount == 0) {
         return;
     }
 
     if (ledCount > MAX_LEDS) {
         ledCount = MAX_LEDS;
     }
 
     _ledCount = ledCount;
     for (size_t i = 0; i < _ledCount; i++) {
         _pins[i] = ledPins[i];
         _states[i] = {};
         platform_gpio_pin_mode(_pins[i], PLATFORM_GPIO_MODE_OUTPUT);
         driveLed(i, false);
     }
 
     _initialized = true;
 }
 
 void StatusDisplay::setReadyPattern(ReadyPattern pattern) {
     if (!_initialized || _ledCount == 0) {
         return;
     }
     applyPattern(0, patternFor(pattern));
 }
 
 void StatusDisplay::setRolePattern(RolePattern pattern) {
     if (!_initialized || _ledCount < 2) {
         return;
     }
     applyPattern(1, patternFor(pattern));
 }
 
 void StatusDisplay::loop() {
     if (!_initialized) {
         return;
     }
 
     uint32_t now = platform_millis();
     for (size_t i = 0; i < _ledCount; i++) {
         const LedPattern* pattern = _states[i].pattern;
         if (!pattern) {
             continue;
         }
 
         if (pattern->isSteady || pattern->stepCount == 0) {
             continue;  // steady pattern already applied
         }
 
         const BlinkStep& step = pattern->steps[_states[i].stepIndex];
         uint32_t elapsed = now - _states[i].lastChangeMs;
         if (elapsed >= step.durationMs) {
             _states[i].stepIndex = (_states[i].stepIndex + 1) % pattern->stepCount;
             const BlinkStep& next = pattern->steps[_states[i].stepIndex];
             driveLed(i, next.levelHigh);
             _states[i].lastChangeMs = now;
         }
     }
 }
 
 const StatusDisplay::LedPattern& StatusDisplay::patternFor(ReadyPattern pattern) {
     static const LedPattern BOOT     = {PATTERN_BOOT_STEPS,     sizeof(PATTERN_BOOT_STEPS) / sizeof(BlinkStep),     false, false};
     static const LedPattern IDLE     = {PATTERN_IDLE_STEPS,     sizeof(PATTERN_IDLE_STEPS) / sizeof(BlinkStep),     false, false};
     static const LedPattern DETECT   = {PATTERN_DETECT_STEPS,   sizeof(PATTERN_DETECT_STEPS) / sizeof(BlinkStep),   false, false};
     static const LedPattern NEGOTIATE= {PATTERN_NEGOTIATE_STEPS,sizeof(PATTERN_NEGOTIATE_STEPS)/ sizeof(BlinkStep), false, false};
     static const LedPattern WAIT_ACK = {PATTERN_WAIT_ACK_STEPS, sizeof(PATTERN_WAIT_ACK_STEPS)/ sizeof(BlinkStep), false, false};
     static const LedPattern EXCHANGE = {PATTERN_EXCHANGE_STEPS, sizeof(PATTERN_EXCHANGE_STEPS)/ sizeof(BlinkStep), false, false};
     static const LedPattern SUCCESS  = {PATTERN_SUCCESS_STEPS,  sizeof(PATTERN_SUCCESS_STEPS)/ sizeof(BlinkStep),  false, false};
     static const LedPattern ERROR    = {PATTERN_ERROR_STEPS,    sizeof(PATTERN_ERROR_STEPS)/ sizeof(BlinkStep),    false, false};
 
     switch (pattern) {
         case ReadyPattern::Booting:      return BOOT;
         case ReadyPattern::Detecting:    return DETECT;
         case ReadyPattern::Negotiating:  return NEGOTIATE;
         case ReadyPattern::WaitingAck:   return WAIT_ACK;
         case ReadyPattern::Exchanging:   return EXCHANGE;
         case ReadyPattern::Success:      return SUCCESS;
         case ReadyPattern::Error:        return ERROR;
         case ReadyPattern::Idle:
         default:
             return IDLE;
     }
 }
 
 const StatusDisplay::LedPattern& StatusDisplay::patternFor(RolePattern pattern) {
     static const LedPattern UNKNOWN = {PATTERN_ROLE_UNKNOWN_STEPS, sizeof(PATTERN_ROLE_UNKNOWN_STEPS) / sizeof(BlinkStep), false, false};
     static const LedPattern MASTER  = {nullptr, 0, true, true};   // steady on
     static const LedPattern SLAVE   = {nullptr, 0, true, false};  // steady off
 
     switch (pattern) {
         case RolePattern::Master:  return MASTER;
         case RolePattern::Slave:   return SLAVE;
         case RolePattern::Unknown:
         default:
             return UNKNOWN;
     }
 }
 
 void StatusDisplay::applyPattern(size_t ledIndex, const LedPattern& pattern) {
     if (ledIndex >= _ledCount) {
         return;
     }
 
     LedState& state = _states[ledIndex];
     if (state.pattern == &pattern) {
         return;  // already active
     }
 
     state.pattern = &pattern;
     state.stepIndex = 0;
     state.lastChangeMs = platform_millis();
 
     if (pattern.isSteady || pattern.stepCount == 0) {
         driveLed(ledIndex, pattern.steadyLevel);
         state.activeLevel = pattern.steadyLevel;
         return;
     }
 
     driveLed(ledIndex, pattern.steps[0].levelHigh);
     state.activeLevel = pattern.steps[0].levelHigh;
 }
 
 void StatusDisplay::driveLed(size_t ledIndex, bool level) {
     if (ledIndex >= _ledCount) {
         return;
     }
     platform_gpio_write(_pins[ledIndex], level ? PLATFORM_GPIO_HIGH : PLATFORM_GPIO_LOW);
 }
