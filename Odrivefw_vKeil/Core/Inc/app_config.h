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
 * 1.  BOARD / HARDWARE (applies to all motor configurations)
 * ===========================================================================*/

/* Hardware version */
#define HW_VERSION_MAJOR        3
#define HW_VERSION_MINOR        4
#define HW_VERSION_VOLTAGE      48

/* ===== TIM1 / TIM8 (APB2 @ 168 MHz) — FOC PWM ===== */
#define TIM_1_8_CLOCK_HZ        168000000
#define TIM_1_8_PERIOD_CLOCKS   10192
#define TIM_1_8_DEADTIME_CLOCKS 20

/* ===== TIM2 (APB1 @ 84 MHz) — AUX ===== */
#define TIM_APB1_CLOCK_HZ       84000000
#define TIM_APB1_PERIOD_CLOCKS  4096
#define TIM_APB1_DEADTIME_CLOCKS 40

/* ===== Current measurement timing ===== */
#define CURRENT_MEAS_PERIOD     ((float)(2*TIM_1_8_PERIOD_CLOCKS)/(float)TIM_1_8_CLOCK_HZ)
#define CURRENT_MEAS_HZ         (TIM_1_8_CLOCK_HZ/(2*TIM_1_8_PERIOD_CLOCKS))

/* ===== DC bus voltage divider (HW VERSION_VOLTAGE = 48 V board) ===== */
#define VBUS_S_DIVIDER_RATIO    19.0f

/* ===== HAL / motor-control timebase timer ===== */
#define TIM_TIME_BASE           TIM14

/* ===== Brake / regeneration resistor ===== */
#define APP_BRAKE_RESISTANCE    0.47f    /* [Ohm] */

/* ===== DC-bus protection ===== */
#define APP_DC_BUS_BROWNOUT_V   8.0f     /* [V] brown-out trip   */
#define APP_VBUS_MAX            52.0f    /* [V] software OV limit */

/* ===== GPIO pin aliases — bridge CubeMX label to firmware macro names =====
 * CubeMX labels the pin M0_ENCZ (PC9). The motor-control firmware was written
 * with M0_DC_CAL_Pin / M0_DC_CAL_GPIO_Port.  The alias below lets the old
 * code compile without modification.
 * NOTE: PC9 is initialised as GPIO_Output by CubeMX; hall_gpio_init()
 *       reconfigures it to Input at run-time (same behaviour as File 1).
 * ========================================================================= */
#define M0_DC_CAL_Pin           M0_ENCZ_Pin        /* PC9,  GPIOC */
#define M0_DC_CAL_GPIO_Port     M0_ENCZ_GPIO_Port

/* M1 Hall-C = PC15 (M1_ENCZ_Pin / M1_ENCZ_GPIO_Port, defined by CubeMX) */

/* Step/Direction interface pins (same physical mapping as File 1 / 5065 board)
 *   GPIO_1 = PA0  (M0 step)   GPIO_2 = PA1  (M0 dir)
 *   GPIO_3 = PA2  (M1 step)   GPIO_4 = PA3  (M1 dir)
 *   M0_ENC_Z     = PA15
 */
#define GPIO_1_Pin              GPIO_PIN_0
#define GPIO_1_GPIO_Port        GPIOA
#define GPIO_2_Pin              GPIO_PIN_1
#define GPIO_2_GPIO_Port        GPIOA
#define GPIO_3_Pin              GPIO_PIN_2
#define GPIO_3_GPIO_Port        GPIOA
#define GPIO_4_Pin              GPIO_PIN_3
#define GPIO_4_GPIO_Port        GPIOA
#define M0_ENC_Z_Pin            GPIO_PIN_15
#define M0_ENC_Z_GPIO_Port      GPIOA

/* ===========================================================================
 * 2.  MOTOR PRESET: 4250
 * ===========================================================================*/
#if MOTOR_CONFIG == 1

    /* --- Motor mechanics -------------------------------------------------- */
    #define APP_POLE_PAIRS               7
    #define APP_HALL_ANGLE_DEG           60

    /* --- FOC / sensorless ------------------------------------------------- */
    #define APP_PM_FLUX_LINKAGE          1.43e-3f
    #define APP_OBSERVER_GAIN            1000.0f
    #define APP_SPIN_UP_CURRENT          10.0f
    #define APP_SPIN_UP_ACCELERATION     400.0f
    #define APP_SPIN_UP_TARGET_VEL       400.0f

    /* --- Calibration ------------------------------------------------------ */
    #define APP_CALIBRATION_CURRENT      3.0f
    #define APP_RESISTANCE_CALIB_MAXV    1.0f

    /* --- Current limits --------------------------------------------------- */
    #define APP_CURRENT_LIM              20.0f

    /* --- Velocity / position PID ----------------------------------------- */
    #define APP_VEL_GAIN                 (15.0f / 1000.0f)
    #define APP_VEL_INTEGRATOR_GAIN      1.0f
    #define APP_VEL_LIMIT                20000.0f
    #define APP_POS_GAIN                 20.0f

/* ===========================================================================
 * 3.  MOTOR PRESET: 5065
 * ===========================================================================
 *  Max power       : 1800 W
 *  Recommended Vdc : 24 – 36 V
 *  kV              : 270 kV
 *  Pole pairs      : 7   (14-pole motor)
 *  pm_flux_linkage = 5.51328895422 / (7 * 270) = 2.92e-3 V/(rad/s)
 * ===========================================================================*/
#elif MOTOR_CONFIG == 2

    /* --- Motor mechanics -------------------------------------------------- */
    #define APP_POLE_PAIRS               7
    #define APP_HALL_ANGLE_DEG           60

    /* --- FOC / sensorless ------------------------------------------------- */
    #define APP_PM_FLUX_LINKAGE          2.92e-3f
    #define APP_OBSERVER_GAIN            1000.0f
    #define APP_SPIN_UP_CURRENT          6.0f
    #define APP_SPIN_UP_ACCELERATION     50.0f
    #define APP_SPIN_UP_TARGET_VEL       250.0f

    /* --- Calibration ------------------------------------------------------ */
    #define APP_CALIBRATION_CURRENT      3.0f
    #define APP_RESISTANCE_CALIB_MAXV    2.0f

    /* --- Current limits --------------------------------------------------- */
    #define APP_CURRENT_LIM              20.0f

    /* --- Velocity / position PID ----------------------------------------- */
    #define APP_VEL_GAIN                 0.01f
    #define APP_VEL_INTEGRATOR_GAIN      0.04f
    #define APP_VEL_LIMIT                20000.0f
    #define APP_POS_GAIN                 20.0f

/* ===========================================================================
 * 4-N.  RESERVED PRESETS
 * ===========================================================================*/
#elif MOTOR_CONFIG == 3
    #error "MOTOR_CONFIG 3 is not yet defined in app_config.h"

#elif MOTOR_CONFIG == 4
    #error "MOTOR_CONFIG 4 is not yet defined in app_config.h"

#else   /* MOTOR_CONFIG == 0 */
    /* No preset — all parameters must be set at run-time via USB/UART */
#endif  /* MOTOR_CONFIG */

/* ===========================================================================
 * 5.  UART PROTOCOL CONFIGURATION
 * ===========================================================================*/

/* Wheel radius used to convert km/h ↔ rad/s (velocity mode) */
#define APP_WHEEL_RADIUS_M      0.025f   /* [m]  5065 rotor radius (50 mm diam / 2) */

/* Torque constant: Kt = 60 / (2*pi*kV) [N·m/A] */
#if MOTOR_CONFIG == 2
    #define APP_KT_NM_PER_A     0.03535f   /* 5065: kV=270 */
#elif MOTOR_CONFIG == 1
    #define APP_KT_NM_PER_A     0.01736f   /* 4250: kV=550 */
#else
    #define APP_KT_NM_PER_A     0.0f
#endif

/* ===========================================================================
 * 6.  HALL SENSOR CONFIGURATION
 * ===========================================================================
 * APP_USE_HALL_SENSOR = 1  → ROTOR_MODE_HALL
 * APP_USE_HALL_SENSOR = 0  → ROTOR_MODE_SENSORLESS
 *
 * Pin assignment (ENC_A/B/Z pads on the board connector):
 *   M0: Hall-A = PB4, Hall-B = PB5, Hall-C = PC9  (M0_ENCZ pad)
 *   M1: Hall-A = PB6, Hall-B = PB7, Hall-C = PC15 (M1_ENCZ pad)
 *
 * Note: hall_gpio_init() reconfigures PC9 and PC15 to GPIO_INPUT at startup.
 * ===========================================================================*/
#if MOTOR_CONFIG == 2
    #define APP_USE_HALL_SENSOR     1
#else
    #define APP_USE_HALL_SENSOR     0
#endif

/* M0 Hall GPIO pins */
#define APP_M0_HALL_A_PORT      GPIOB
#define APP_M0_HALL_A_PIN       GPIO_PIN_4    /* M0_ENCA / PB4  */
#define APP_M0_HALL_B_PORT      GPIOB
#define APP_M0_HALL_B_PIN       GPIO_PIN_5    /* M0_ENCB / PB5  */
#define APP_M0_HALL_C_PORT      GPIOC
#define APP_M0_HALL_C_PIN       GPIO_PIN_9    /* M0_ENCZ / PC9  (Hall-C) */

/* Hall calibration — M0 */
#define APP_M0_HALL_MOTOR_DIR   1
#define APP_M0_HALL_PHASE_OFFSET 3.5f

/* Application spin direction (independent of FOC calibration):
 *  1 = positive command → hardware-forward spin
 * -1 = positive command → hardware-backward spin */
#define APP_M0_VEL_DIRECTION    (-1)
#define APP_M1_VEL_DIRECTION      1

#if APP_USE_HALL_SENSOR
/* M1 Hall GPIO pins */
#define APP_M1_HALL_A_PORT      GPIOB
#define APP_M1_HALL_A_PIN       GPIO_PIN_6    /* M1_ENCA / PB6  */
#define APP_M1_HALL_B_PORT      GPIOB
#define APP_M1_HALL_B_PIN       GPIO_PIN_7    /* M1_ENCB / PB7  */
#define APP_M1_HALL_C_PORT      GPIOC
#define APP_M1_HALL_C_PIN       GPIO_PIN_15   /* M1_ENCZ / PC15 (Hall-C) */

/* Hall calibration — M1 */
#define APP_M1_HALL_MOTOR_DIR   1
#define APP_M1_HALL_PHASE_OFFSET 3.5f
#endif /* APP_USE_HALL_SENSOR */

/* Hall PLL bandwidth [rad/s] */
#define APP_HALL_PLL_BANDWIDTH  200.0f

/* Hall startup feedforward */
#define APP_M0_HALL_STARTUP_CURRENT  2.0f    /* [A] */
#define APP_M1_HALL_STARTUP_CURRENT  3.0f    /* [A] */
#define APP_HALL_STARTUP_VEL_THRESH  5.0f    /* [rad/s] */

/* Legacy alias */
#define APP_HALL_STARTUP_CURRENT     APP_M0_HALL_STARTUP_CURRENT

#endif /* __APP_CONFIG_H */
