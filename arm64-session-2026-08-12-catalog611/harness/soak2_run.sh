#!/system/bin/sh
cd /data/local/tmp/asx
mkdir -p /data/local/tmp/soak616v2
T=/data/local/tmp/noice_tap
X=/data/local/tmp/noice_text
S() { snapshot_display -f /data/local/tmp/soak616v2/$1.jpeg >/dev/null 2>&1; }
echo V2-START $(date) > /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Divider" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "677 1561" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 001_Divider_enter
echo "1152 64" > $T; sleep 3
S 002_Divider_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "done Divider $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Elevation_and_Shadow" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "174 1271" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 003_Elevation_and_Shadow_enter
echo "1152 64" > $T; sleep 3
S 004_Elevation_and_Shadow_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "80 204" > $T; sleep 3
S 005_Elevation_and_Shadow_w01_NavigationRailItemView
echo back > $T; sleep 2
echo "80 325" > $T; sleep 3
S 006_Elevation_and_Shadow_w02_NavigationRailItemView
echo back > $T; sleep 2
echo "80 447" > $T; sleep 3
S 007_Elevation_and_Shadow_w03_NavigationRailItemView
echo back > $T; sleep 2
echo "679 230" > $T; sleep 3
S 008_Elevation_and_Shadow_w04_MaterialButton
echo back > $T; sleep 2
echo "679 326" > $T; sleep 3
S 009_Elevation_and_Shadow_w05_MaterialButton
echo back > $T; sleep 2
echo "679 422" > $T; sleep 3
S 010_Elevation_and_Shadow_w06_MaterialButton
echo back > $T; sleep 2
echo "768 518" > $T; sleep 3
S 011_Elevation_and_Shadow_w07_AppCompatSpinner
echo back > $T; sleep 2
echo "done Elevation_and_Shadow $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Floating_Action_Button" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "777 1271" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 012_Floating_Action_Button_enter
echo "1152 64" > $T; sleep 3
S 013_Floating_Action_Button_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 428" > $T; sleep 3
S 014_Floating_Action_Button_w01_MaterialSwitch
echo back > $T; sleep 2
echo "502 575" > $T; sleep 3
echo wl616 > $X; sleep 3
S 015_Floating_Action_Button_w02_AppCompatEditText
echo back > $T; sleep 2
echo "1070 584" > $T; sleep 3
S 016_Floating_Action_Button_w03_MaterialButton
echo back > $T; sleep 2
echo "done Floating_Action_Button $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Image_View" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "106 1631" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 017_Image_View_enter
echo "600 421" > $T; sleep 3
S 018_Image_View_w00_ConstraintLayout
echo back > $T; sleep 2
echo "1120 421" > $T; sleep 3
S 019_Image_View_w01_MaterialButton
echo back > $T; sleep 2
echo "600 642" > $T; sleep 3
S 020_Image_View_w02_ConstraintLayout
echo back > $T; sleep 2
echo "1120 642" > $T; sleep 3
S 021_Image_View_w03_MaterialButton
echo back > $T; sleep 2
echo "1152 64" > $T; sleep 3
S 022_Image_View_w04_ActionMenuItemView
echo back > $T; sleep 2
echo "done Image_View $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Navigation_Rail" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "129 1701" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 023_Navigation_Rail_enter
echo "1152 64" > $T; sleep 3
S 024_Navigation_Rail_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 025_Navigation_Rail_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 026_Navigation_Rail_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 027_Navigation_Rail_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 028_Navigation_Rail_w04_TextInputEditText
echo back > $T; sleep 2
echo "done Navigation_Rail $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Progress_Indicator" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "748 1701" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 029_Progress_Indicator_enter
echo "1152 64" > $T; sleep 3
S 030_Progress_Indicator_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 031_Progress_Indicator_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 032_Progress_Indicator_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 033_Progress_Indicator_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 034_Progress_Indicator_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 035_Progress_Indicator_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 036_Progress_Indicator_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 037_Progress_Indicator_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 038_Progress_Indicator_w08_MaterialSwitch
echo back > $T; sleep 2
echo "done Progress_Indicator $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Radio_Button" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "116 1411" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 039_Radio_Button_enter
echo "1152 64" > $T; sleep 3
S 040_Radio_Button_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 041_Radio_Button_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 042_Radio_Button_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 043_Radio_Button_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 044_Radio_Button_w04_TextInputEditText
echo back > $T; sleep 2
echo "done Radio_Button $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Search" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "675 1411" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 045_Search_enter
echo "1152 64" > $T; sleep 3
S 046_Search_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 047_Search_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 048_Search_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 049_Search_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 050_Search_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 051_Search_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 052_Search_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 053_Search_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 054_Search_w08_MaterialSwitch
echo back > $T; sleep 2
echo "done Search $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Shape_Theming" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "131 1121" > $T; sleep 7
echo "600 721" > $T; sleep 8
S 055_Shape_Theming_enter
echo "1152 64" > $T; sleep 3
S 056_Shape_Theming_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "400 272 800 272" > $T; sleep 3
S 057_Shape_Theming_w01_Slider
echo back > $T; sleep 2
echo "400 432 800 432" > $T; sleep 3
S 058_Shape_Theming_w02_RangeSlider
echo back > $T; sleep 2
echo "120 560" > $T; sleep 3
S 059_Shape_Theming_w03_MaterialButton
echo back > $T; sleep 2
echo "done Shape_Theming $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Slider" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "68 1481" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 060_Slider_enter
echo "1152 64" > $T; sleep 3
S 061_Slider_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 062_Slider_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 063_Slider_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 064_Slider_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 065_Slider_w04_TextInputEditText
echo back > $T; sleep 2
echo "done Slider $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Switch" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "674 1481" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 066_Switch_enter
echo "1152 64" > $T; sleep 3
S 067_Switch_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 068_Switch_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 069_Switch_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 070_Switch_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 071_Switch_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 072_Switch_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 073_Switch_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 074_Switch_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 075_Switch_w08_MaterialSwitch
echo back > $T; sleep 2
echo "done Switch $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Tabs" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "61 1191" > $T; sleep 7
echo "600 721" > $T; sleep 8
S 076_Tabs_enter
echo "1152 64" > $T; sleep 3
S 077_Tabs_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "400 272 800 272" > $T; sleep 3
S 078_Tabs_w01_Slider
echo back > $T; sleep 2
echo "400 432 800 432" > $T; sleep 3
S 079_Tabs_w02_RangeSlider
echo back > $T; sleep 2
echo "120 560" > $T; sleep 3
S 080_Tabs_w03_MaterialButton
echo back > $T; sleep 2
echo "done Tabs $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Time_Picker" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "106 1551" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 081_Time_Picker_enter
echo "1152 64" > $T; sleep 3
S 082_Time_Picker_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 083_Time_Picker_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 084_Time_Picker_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 085_Time_Picker_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 086_Time_Picker_w04_TextInputEditText
echo back > $T; sleep 2
echo "done Time_Picker $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Top_App_Bar" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "711 1551" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 087_Top_App_Bar_enter
echo "1152 64" > $T; sleep 3
S 088_Top_App_Bar_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 089_Top_App_Bar_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 090_Top_App_Bar_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 091_Top_App_Bar_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 092_Top_App_Bar_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 093_Top_App_Bar_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 094_Top_App_Bar_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 095_Top_App_Bar_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 096_Top_App_Bar_w08_MaterialSwitch
echo back > $T; sleep 2
echo "done Top_App_Bar $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "== Transition" >> /data/local/tmp/soak616v2/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "94 1719" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 097_Transition_enter
echo "600 421" > $T; sleep 3
S 098_Transition_w00_ConstraintLayout
echo back > $T; sleep 2
echo "1120 421" > $T; sleep 3
S 099_Transition_w01_MaterialButton
echo back > $T; sleep 2
echo "600 642" > $T; sleep 3
S 100_Transition_w02_ConstraintLayout
echo back > $T; sleep 2
echo "1120 642" > $T; sleep 3
S 101_Transition_w03_MaterialButton
echo back > $T; sleep 2
echo "600 786" > $T; sleep 3
S 102_Transition_w04_ConstraintLayout
echo back > $T; sleep 2
echo "1120 786" > $T; sleep 3
S 103_Transition_w05_MaterialButton
echo back > $T; sleep 2
echo "600 930" > $T; sleep 3
S 104_Transition_w06_ConstraintLayout
echo back > $T; sleep 2
echo "1120 930" > $T; sleep 3
S 105_Transition_w07_MaterialButton
echo back > $T; sleep 2
echo "600 1074" > $T; sleep 3
S 106_Transition_w08_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1074" > $T; sleep 3
S 107_Transition_w09_MaterialButton
echo back > $T; sleep 2
echo "600 1218" > $T; sleep 3
S 108_Transition_w10_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1218" > $T; sleep 3
S 109_Transition_w11_MaterialButton
echo back > $T; sleep 2
echo "600 1362" > $T; sleep 3
S 110_Transition_w12_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1362" > $T; sleep 3
S 111_Transition_w13_MaterialButton
echo back > $T; sleep 2
echo "600 1506" > $T; sleep 3
S 112_Transition_w14_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1506" > $T; sleep 3
S 113_Transition_w15_MaterialButton
echo back > $T; sleep 2
echo "600 1650" > $T; sleep 3
S 114_Transition_w16_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1650" > $T; sleep 3
S 115_Transition_w17_MaterialButton
echo back > $T; sleep 2
echo "1152 64" > $T; sleep 3
S 116_Transition_w18_ActionMenuItemView
echo back > $T; sleep 2
echo "done Transition $(date +%H:%M:%S)" >> /data/local/tmp/soak616v2/soak.log
echo V2-END $(date) >> /data/local/tmp/soak616v2/soak.log
