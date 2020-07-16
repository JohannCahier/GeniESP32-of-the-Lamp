#ifndef _GENIUS_I2C_LAMP_PROTOCOL_H_
#define _GENIUS_I2C_LAMP_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif


esp_err_t i2c_master_init(void);
esp_err_t i2c_slave_init(void);

// To send heartbeat frames, heartbeat(0) and heartbeat(1)
esp_err_t send_i2c_heartbeat(uint8_t value);

// send the config/init frame...
esp_err_t send_i2c_config(void);

// send the ambient light frame
esp_err_t send_i2c_ambiant_light(int16_t intensity);

// send button frames
// tempo is the delay during which the button has been pressed.
//       unit is 1/10 s
// NOTE : if tempo==1, it's a 'click' (switch the light on/off)
//        if tempo in [2..255] it mean a "long press" (0.2 to 25.5s)
//        a "long press" will change light intensity (either increasing or decreasing it).


//         esp_err_t send_i2c_button_down(void);
//         esp_err_t send_i2c_button_up(uint8_t tempo);


#ifdef __cplusplus
}
#endif



#endif /*_GENIUS_I2C_LAMP_PROTOCOL_H_*/
