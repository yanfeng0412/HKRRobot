/* =============================================================================
 * app_config.h  —  Application-level motor & board configuration
 * =============================================================================
 *
 * Motor selection:
 *   MOTOR_CONFIG  0  = no preset (use defaults in low_level.c)
 *   MOTOR_CONFIG  1  = 4250 motor
 *   MOTOR_CONFIG  2  = 5065 motor
 *   3, 4 ...          reserved for future motors
 *
 * Set only ONE of the values below to 1.
 */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ---------------------------------------------------------------------------
 * Motor selection switch.  Change this value to load a different motor preset.
 * ---------------------------------------------------------------------------*/
#define MOTOR_CONFIG  2   /* 0=none, 1=4250, 2=5065 */

/* ===========================================================================
 * 1.  BOARD / POWER STAGE  (apply to all motors)
 * ===========================================================================*/

/* Brake / regeneration resistor – ohms */
#define APP_BRAKE_RESISTANCE         0.47f   /* [Ohm] */

/* DC-bus brown-out trip level */
#define APP_DC_BUS_BROWNOUT_V        8.0f    /* [V]   */

/* Maximum allowable DC bus voltage (software limit) */
#define APP_VBUS_MAX                 52.0f   /* [V]   */

/* ===========================================================================
 * 2.  MOTOR PRESET: 4250
 * ===========================================================================
 * Typical 4250 outrunner used on this board (original ODrive default).
 *
 *  pole pairs      : 7   (14-pole motor)
 *  kV              : 550 kV
 *  pm_flux_linkage : 5.51328895422 / (7 * 550) = 1.43e-3 V/(rad/s)
 *  Hall sensor deg : 60
 * ===========================================================================*/
#if MOTOR_CONFIG == 1

    /* --- Motor mechanics -------------------------------------------------- */
    #define APP_POLE_PAIRS               7        /* pole-pair count           */
    #define APP_HALL_ANGLE_DEG           60        /* Hall sensor phase angle   */

    /* --- FOC / sensorless ------------------------------------------------- */
    #define APP_PM_FLUX_LINKAGE          1.43e-3f  /* [V/(rad/s)]               */
    #define APP_OBSERVER_GAIN            1000.0f   /* [rad/s]                   */
    #define APP_SPIN_UP_CURRENT          10.0f     /* [A]                       */
    #define APP_SPIN_UP_ACCELERATION     400.0f    /* [rad/s^2]                 */
    #define APP_SPIN_UP_TARGET_VEL       400.0f    /* [rad/s]                   */

    /* --- Calibration ------------------------------------------------------ */
    #define APP_CALIBRATION_CURRENT      3.0f      /* [A]  lowered: 10A was tripping supply current limit */
    #define APP_RESISTANCE_CALIB_MAXV    1.0f      /* [V]                       */

    /* --- Current limits --------------------------------------------------- */
    #define APP_CURRENT_LIM              20.0f     /* [A]  soft limit           */

    /* --- Velocity / position PID ----------------------------------------- */
    #define APP_VEL_GAIN                 (15.0f / 1000.0f)  /* [A/(rad/s)]      */
    #define APP_VEL_INTEGRATOR_GAIN      1.0f      /* [A/(rad/s*s)]             */
    #define APP_VEL_LIMIT                20000.0f  /* [counts/s]                */
    #define APP_POS_GAIN                 20.0f     /* [(counts/s)/counts]       */

/* ===========================================================================
 * 3.  MOTOR PRESET: 5065
 * ===========================================================================
 * 5065 brushless outrunner specification:
 *  Max power       : 1800 W
 *  Recommended Vdc : 24 – 36 V
 *  kV              : 270 kV
 *  Pole pairs      : 7   (14-pole motor)
 *  No-load current : 0.5 – 1 A
 *  No-load speed   : 6480 – 9600 rpm  (@ 24 – 36 V)
 *  Hall sensor deg : 60
 *  Rated torque    : 1.5 – 2 N·m
 *
 *  pm_flux_linkage = 5.51328895422 / (7 * 270) = 2.92e-3 V/(rad/s)
 *
 *  Peak current estimate: 1800 W / 24 V ≈ 75 A
 *  Conservative soft current limit: 40 A
 *  Calibration current: 15 A (safe for initial measurement)
 * ===========================================================================*/
#elif MOTOR_CONFIG == 2

    /* --- Motor mechanics -------------------------------------------------- */
    #define APP_POLE_PAIRS               7         /* pole-pair count (14-pole motor)   */
    #define APP_HALL_ANGLE_DEG           60        /* Hall sensor phase angle   */

    /* --- FOC / sensorless ------------------------------------------------- */
    /* pm_flux_linkage = 5.51328895422 / (pole_pairs * kV) = 5.51328895422 / (7*270) = 2.92e-3 */
    #define APP_PM_FLUX_LINKAGE          2.92e-3f  /* [V/(rad/s)]               */
    #define APP_OBSERVER_GAIN            1000.0f   /* [rad/s]                   */
    #define APP_SPIN_UP_CURRENT          6.0f      /* [A]  increased from 4A: more pull torque for reliable startup */
    #define APP_SPIN_UP_ACCELERATION     50.0f     /* [rad/s^2]  slow ramp so rotor can follow           */
    #define APP_SPIN_UP_TARGET_VEL       250.0f    /* [rad/s]  higher: stronger BEMF → better observer SNR at handoff */

    /* --- Calibration ------------------------------------------------------ */
    #define APP_CALIBRATION_CURRENT      3.0f      /* [A]  lowered: 10A was tripping supply current limit */
    #define APP_RESISTANCE_CALIB_MAXV    2.0f      /* [V]  */

    /* --- Current limits --------------------------------------------------- */
    #define APP_CURRENT_LIM              20.0f     /* [A]  (was 40, ODrive uses 20) */

    /* --- Velocity / position PID ----------------------------------------- */
    /* vel_gain: 0.02 A/(rad/s) → P term = 1.11 A at 5 km/h error (enough to start motor)
     * vel_integrator_gain: 0.01 → reduced from 0.04 to avoid PID chasing Hall quantisation
     *   noise (CPR=6 → 6 velocity samples/rev, high integrator gain amplifies jitter). */
//    #define APP_VEL_GAIN                 0.02f     /* [A/(rad/s)]  (was 0.01)   */
//    #define APP_VEL_INTEGRATOR_GAIN      0.01f     /* [A/(rad/s·s)] (was 0.04, reduced to reduce vibration) */
    #define APP_VEL_GAIN                 0.01f     /* [A/(rad/s)]  (was 0.01)   */
    #define APP_VEL_INTEGRATOR_GAIN      0.04f     /* [A/(rad/s·s)] (was 0.04, reduced to reduce vibration) */
    #define APP_VEL_LIMIT                20000.0f  /* [counts/s]                */
    #define APP_POS_GAIN                 20.0f     /* [(counts/s)/counts]       */

/* ===========================================================================
 * 4-N.  RESERVED PRESETS
 * ===========================================================================*/
#elif MOTOR_CONFIG == 3
    /* TODO: add motor 3 parameters */
    #error "MOTOR_CONFIG 3 is not yet defined in app_config.h"

#elif MOTOR_CONFIG == 4
    /* TODO: add motor 4 parameters */
    #error "MOTOR_CONFIG 4 is not yet defined in app_config.h"

#else   /* MOTOR_CONFIG == 0  — no preset, all parameters must be set by USB */
    /* Leave all #defines undefined; low_level.c compiled defaults will apply   */
#endif  /* MOTOR_CONFIG */

/* ===========================================================================
 * 5.  SIMPLE UART PROTOCOL CONFIGURATION
 * ===========================================================================
 * Frame delimiters (ASCII text protocol):
 *   PC → Board :  $<channel>,<mode>,<value>;\n
 *   Board → PC :  [<channel>,<mode>,<value>,<status>]\n
 *
 * Channel  : "M0" | "M1"
 * Mode     : "V"  velocity   – value in km/h
 *            "P"  position   – value in degrees
 *            "T"  torque     – value in N·m
 *            "I"  current    – value in A
 *            "H"  halt       – value ignored (stops motor)
 *            "Q"  query      – returns current state
 * ===========================================================================*/

/* Wheel radius used to convert km/h ↔ rad/s (velocity mode) */
/* Set to 0 to disable km/h conversion and use raw rad/s       */
/* 5065 motor naming: 50 mm outer diameter → rotor radius = 0.025 m */
#define APP_WHEEL_RADIUS_M           0.025f   /* [m]  5065 rotor radius (50mm diam / 2) */

/* Torque constant: Kt = 60 / (2*pi*kV) [N·m/A]               */
/* Used to convert N·m setpoint to current setpoint            */
#if MOTOR_CONFIG == 2
    /* 5065: kV=270  →  Kt = 60 / (2*pi*270) = 0.03535 N·m/A  */
    #define APP_KT_NM_PER_A          0.03535f
#elif MOTOR_CONFIG == 1
    /* 4250: kV=550  →  Kt = 60 / (2*pi*550) = 0.01736 N·m/A  */
    #define APP_KT_NM_PER_A          0.01736f
#else
    #define APP_KT_NM_PER_A          0.0f   /* torque mode disabled without preset */
#endif

/* ===========================================================================
 * 6.  HALL SENSOR CONFIGURATION
 * ===========================================================================
 * APP_USE_HALL_SENSOR = 1  selects ROTOR_MODE_HALL instead of sensorless.
 * APP_USE_HALL_SENSOR = 0  selects ROTOR_MODE_SENSORLESS (FOC, no encoder).
 *
 * 5065 first-run: set to 0 (sensorless) for initial spin-up validation.
 * Switch to 1 once Hall wiring is confirmed correct.
 *
 * GPIO pin assignment (reuses encoder A/B/Z pads on the board connector):
 *   Board M0 connector: PB4=ENC_A, PB5=ENC_B, PA15=ENC_Z
 *   Wire to motor:      Hall-A=PB4,  Hall-B=PB5,  Hall-C=PA15
 *
 * Note: hall_gpio_init() reconfigures these pins to GPIO_INPUT at startup,
 *       which disables TIM3 encoder counting. Keep ROTOR_MODE_ENCODER
 *       disabled (APP_USE_HALL_SENSOR 0) if you need TIM3 instead.
 * ===========================================================================*/
#if MOTOR_CONFIG == 2
    #define APP_USE_HALL_SENSOR      1  /* 0 = sensorless; 1 = Hall closed-loop */
#else
    #define APP_USE_HALL_SENSOR      0
#endif

/* Hall GPIO pins – wired to M0 connector (ENC_A/ENC_B/ENC_Z pads) */
#define APP_HALL_A_PORT        GPIOB
#define APP_HALL_A_PIN         GPIO_PIN_4    /* M0_ENC_A / PB4 */
#define APP_HALL_B_PORT        GPIOB
#define APP_HALL_B_PIN         GPIO_PIN_5    /* M0_ENC_B / PB5 */
#define APP_HALL_C_PORT        GPIOC
#define APP_HALL_C_PIN         GPIO_PIN_9    /* Hall-C / PC9 (ENC_Z connector pad on this board) */

/* Hall direction (1 = forward, -1 = reverse) and electrical sector offset.
 * phase_offset is in Hall counts; fractional values are allowed, e.g. 0.5f
 * means the center of a 60 electrical degree Hall sector.
 *
 * WARNING: motor_dir affects BOTH the FOC phase angle AND the velocity sign.
 * It is a FOC calibration value – do NOT change it to reverse the application
 * spin direction.  Use APP_M0_VEL_DIRECTION / APP_M1_VEL_DIRECTION for that. */
#define APP_HALL_MOTOR_DIR     1       /* FOC calibration – do not change to reverse spin */
#define APP_HALL_PHASE_OFFSET  3.5f

/* Application-level spin direction (independent of FOC calibration).
 *  1 = positive command → motor spins in the hardware-forward direction
 * -1 = positive command → motor spins in the hardware-backward direction
 * Flip these to reverse output direction without touching FOC tuning. */
#define APP_M0_VEL_DIRECTION   (-1)   /* M0 output direction reversed */
#define APP_M1_VEL_DIRECTION     1    /* M1 normal */

#if APP_USE_HALL_SENSOR
/* M1 Hall GPIO pins – wired to M1 connector (ENC_A=PB6, ENC_B=PB7, ENC_Z=PC15) */
#define APP_M1_HALL_A_PORT       GPIOB
#define APP_M1_HALL_A_PIN        GPIO_PIN_6    /* M1_ENC_A / PB6 */
#define APP_M1_HALL_B_PORT       GPIOB
#define APP_M1_HALL_B_PIN        GPIO_PIN_7    /* M1_ENC_B / PB7 */
#define APP_M1_HALL_C_PORT       GPIOC
#define APP_M1_HALL_C_PIN        GPIO_PIN_15   /* M1_ENC_Z / PC15 (M1_DC_CAL pad repurposed) */

/* M1 direction and phase offset – adjust after first-run validation.
 *
 * CALIBRATION PROCEDURE (run after wiring M1 Hall sensors):
 *   1. Set APP_M1_HALL_MOTOR_DIR = 1 and APP_M1_HALL_PHASE_OFFSET = 3.5
 *   2. Send $M1,V,1.0; (slow speed)  – watch $ST M1h field.
 *   3. If M1h cycles in order 1→3→2→6→4→5 → motor_dir=1 is correct.
 *      If M1h cycles in reverse order 5→4→6→2→3→1 → set motor_dir=-1.
 *   4. If motor runs away or vibrates: the phase_offset is wrong.
 *      Try values 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 until motor runs smoothly.
 *
 * Symptom guide:
 *   "越转越快" (runaway):  motor_dir wrong OR phase_offset wrong
 *   Motor only oscillates: phase_offset is off (try all 6 values)
 *   Motor runs backward:   negate motor_dir
 *   Stall error (code 24): motor stalled 3 s → check motor_dir first
 */
#define APP_M1_HALL_MOTOR_DIR      1      /* 1 or -1 – verify with first run */
#define APP_M1_HALL_PHASE_OFFSET   3.5f  /* 0.5/1.5/2.5/3.5/4.5/5.5 – try if motor oscillates */
#endif /* APP_USE_HALL_SENSOR */

/* Hall PLL bandwidth [rad/s].
 * Coarser than incremental encoder (CPR=6 vs 2400).
 * Rule: ~2× the max expected electrical frequency.
 * 5065 at 3000 rpm → ω_e = 3000/60 × 2π × 7 ≈ 2199 rad/s; use 200 rad/s. */
#define APP_HALL_PLL_BANDWIDTH  200.0f       /* [rad/s] */

/* Hall startup feedforward parameters.
 * When velocity is commanded but motor is nearly stationary, guarantee at least
 * APP_HALLx_STARTUP_CURRENT amps of Iq to overcome cogging torque and static friction.
 * Applies only when Hall state is valid (state 1-6) and |pll_vel| < STARTUP_VEL_THRESH.
 * Set to 0.0f to disable feedforward for that channel.
 *
 * M1 default is higher than M0 because the Hall phase_offset may not be perfectly
 * calibrated for M1, leading to phase error θ ≠ 0 → effective torque = Iq×cos(θ).
 * Raising M1 startup Iq compensates without touching FOC calibration.
 * Once APP_M1_HALL_PHASE_OFFSET is fine-tuned under load, lower this back toward 2.0f. */
#define APP_M0_HALL_STARTUP_CURRENT  2.0f    /* [A]  M0 minimum startup Iq           */
#define APP_M1_HALL_STARTUP_CURRENT  3.0f    /* [A]  M1 higher: compensates phase offset uncertainty */
#define APP_HALL_STARTUP_VEL_THRESH  5.0f    /* [rad/s] "near-standstill" threshold  */

/* Legacy alias so any code still using the old single-channel macro still compiles */
#define APP_HALL_STARTUP_CURRENT     APP_M0_HALL_STARTUP_CURRENT

#endif /* __APP_CONFIG_H */
