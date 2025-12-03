import requests
import streamlit as st
import time

esp32_ip = "REPLACE WITH YOUR ESP32 IP ADDRESS" # Esp32 IP Address

# Door Button Toggle
if "door_toggle" not in st.session_state:
    st.session_state.door_toggle = True
# Bed Button Toggle
if "bed_toggle" not in st.session_state:
    st.session_state.bed_toggle = True
# Chamber Fan Button Toggle
if "chamber_fan_toggle" not in st.session_state:
    st.session_state.chamber_fan_toggle = True
# Cooling Fan Button Toggle 
if "cooling_fan_toggle" not in st.session_state:
    st.session_state.cooling_fan_toggle = True
# UVC LEDs Button Toggle
if "uvc_led_toggle" not in st.session_state:
    st.session_state.uvc_led_toggle = True

def main(): 
    st.title("SHOE DISPLAY DEBUG UI")
    open_col, close_col = st.columns(2, vertical_alignment="bottom")
    # Open Door Column
    with open_col:
        st.subheader("SERVO CONTROL")
        if st.session_state.door_toggle == True:
            if st.button("OPEN DOOR", type="primary", icon="🚪"):
                message = requests.get(f"http://{esp32_ip}/doorOpen")
                if message.text == "Door is open":
                    st.toast("Door Successfully Opened", icon="✅")
                    time.sleep(1)
                    st.session_state.door_toggle = False
                    print(message.text)
                    st.rerun()
        else:
            if st.button("CLOSE DOOR", type="secondary", icon="🚪"):
                message = requests.get(f"http://{esp32_ip}/doorClose")
                if message.text == "Door is closed":
                    st.toast("Door Successfully Closed", icon="❌")
                    time.sleep(1)
                    st.session_state.door_toggle = True
                    print(message.text)
                    st.rerun()

        

    with close_col:
        st.subheader("BED CONTROL")
        if st.session_state.bed_toggle == True:
            if st.button("SLIDE IN BED", type="primary", icon="🛏️"):
                message = requests.get(f"http://{esp32_ip}/bedSlideIn")
                if message.text:
                    st.toast("The Bed is Inside", icon="❌")
                    time.sleep(1)
                    st.session_state.bed_toggle = False
                    print(message.text)
                    st.rerun()
        else:
            if st.button("SLIDE OUT BED", type="secondary", icon="🛏️"):
                message = requests.get(f"http://{esp32_ip}/bedSlideOut")
                if message.text:
                    st.toast("The Bed is Outside", icon="✅")
                    time.sleep(1)
                    st.session_state.bed_toggle = True
                    print(message.text)
                    st.rerun()

    # LED Light Control
    # Button Column Setuo
    st.subheader("FAN AND UVC LED CONTROL")
    chamber_fan, cooling_fan, uvc_leds = st.columns(3, vertical_alignment="bottom")

    # Chamber Fan
    with chamber_fan:
        if st.session_state.chamber_fan_toggle == True:
            if st.button("Turn On Chamber Fan", type="primary", icon="💨"):
                message = requests.get(f"http://{esp32_ip}/chamberFanOn")
                if message.text:
                    st.toast("The Inside Chamber Fan is On", icon="✅")
                    time.sleep(1);
                    st.session_state.chamber_fan_toggle = False
                    print(message.text)
                    st.rerun()
        else:
            if st.button("Turn Off Chamber Fan", type="secondary", icon="💨"):
                message = requests.get(f"http://{esp32_ip}/chamberFanOff")
                if message.text:
                    st.toast("The Inside Chamber Fan is Off", icon="❌")
                    time.sleep(1);
                    st.session_state.chamber_fan_toggle = True
                    print(message.text)
                    st.rerun()
    
    # Cooling Fan 
    with cooling_fan:
        if st.session_state.cooling_fan_toggle == True:
            if st.button("Turn On Cooling Fan", type="primary", icon="❄️"):
                message = requests.get(f"http://{esp32_ip}/coolingFanOn")
                if message.text:
                    st.toast("The Cooling Fan is On", icon="✅")
                    time.sleep(1);
                    st.session_state.cooling_fan_toggle = False
                    print(message.text)
                    st.rerun()
        else:
            if st.button("Turn Off Cooling Fan", type="secondary", icon="❄️"):
                message = requests.get(f"http://{esp32_ip}/coolingFanOff")
                if message.text:
                    st.toast("The Cooling Fan is Off", icon="❌")
                    time.sleep(1);
                    st.session_state.cooling_fan_toggle = True
                    print(message.text)
                    st.rerun()

    # UVC LEDs
    with uvc_leds:
        if st.session_state.uvc_led_toggle == True:
            if st.button("Turn On UVC LEDs", type="primary", icon="💡"):
                message = requests.get(f"http://{esp32_ip}/uvcLEDsOn")
                if message.text:
                    st.toast("The UVC LEDs Are On", icon="✅")
                    time.sleep(1);
                    st.session_state.uvc_led_toggle = False
                    print(message.text)
                    st.rerun()
        else:
            if st.button("Turn Off UVC LEDs", type="secondary", icon="💡"):
                message = requests.get(f"http://{esp32_ip}/uvcLEDsOff")
                if message.text:
                    st.toast("The UVC LEDs Are Off", icon="❌")
                    time.sleep(1);
                    st.session_state.uvc_led_toggle = True
                    print(message.text)
                    st.rerun()



if __name__ == "__main__":
    main()
