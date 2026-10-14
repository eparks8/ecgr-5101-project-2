# Sleep/Play Integration: Open Questions (Aravind)

1. @eparks8 @JonHemrick @SamagaioA @k-singam: Play uses a breadboard pushbutton on PF13 (Zio D7, CN10 pin 2) to GND. Please don't use PF13 for anything else.

2. @JonHemrick: Sleep uses a photoresistor. Your pot is on PA3 (A0). The "PA0" comment in stm32l4xx_hal_msp.c looks like a typo for A0. Can you confirm the pot is physically on A0?

3. @JonHemrick: To avoid touching your ADC1 setup, I'd like to put the LDR on A2 (PC3) and read it with ADC2 instead. Any objection?

4. Which branch should we integrate into? I'll add Sleep/Play to menu.c and CMakeLists once we agree (@SamagaioA, I'll coordinate the menu rows with you).

5. @eparks8: Sleep needs a closed-eyes face. Could you add PET_MOOD_SLEEPY to PetMood_t and the pet renderer? If not, I'll draw closed eyes myself and need the eye coordinates exported from pet.h.

6. @eparks8: Pet_t only has hunger and happiness. What should sleeping "recover"? My proposal: happiness slowly rises while asleep, and hunger keeps ticking as normal.

7. Proposed Sleep flow: select SLEEP in the menu, the prompt says "cover the light," the pet sleeps with a Zzz animation while the LDR is dark, and it wakes up and returns to the menu when the light comes back. Objections?

My code is on origin/aravind-sleep-play.
