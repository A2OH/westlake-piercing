#!/system/bin/sh
cd /data/local/tmp/asx
mkdir -p /data/local/tmp/soak616
T=/data/local/tmp/noice_tap
X=/data/local/tmp/noice_text
S() { snapshot_display -f /data/local/tmp/soak616/$1.jpeg >/dev/null 2>&1; }
echo SOAK-START $(date) > /data/local/tmp/soak616/soak.log
echo "== Bottom_App_Bar" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "736 411" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 001_Bottom_App_Bar_enter
echo "1152 64" > $T; sleep 3
S 002_Bottom_App_Bar_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "494 267" > $T; sleep 3
S 003_Bottom_App_Bar_w01_MaterialButton
echo back > $T; sleep 2
echo "710 267" > $T; sleep 3
S 004_Bottom_App_Bar_w02_MaterialButton
echo back > $T; sleep 2
echo "599 379" > $T; sleep 3
S 005_Bottom_App_Bar_w03_MaterialSwitch
echo back > $T; sleep 2
echo "56 1840" > $T; sleep 3
S 006_Bottom_App_Bar_w04_AppCompatImageButton
echo back > $T; sleep 2
echo "952 1840" > $T; sleep 3
S 007_Bottom_App_Bar_w05_ActionMenuItemView
echo back > $T; sleep 2
echo "1048 1840" > $T; sleep 3
S 008_Bottom_App_Bar_w06_ActionMenuItemView
echo back > $T; sleep 2
echo "1144 1840" > $T; sleep 3
S 009_Bottom_App_Bar_w07_ActionMenuItemView
echo back > $T; sleep 2
echo "600 1864" > $T; sleep 3
S 010_Bottom_App_Bar_w08_FloatingActionButton
echo back > $T; sleep 2
echo "600 2080" > $T; sleep 3
S 011_Bottom_App_Bar_w09_NavigationMenuItemView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Bottom_App_Bar $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Bottom_Sheet" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "120 771" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 012_Bottom_Sheet_enter
echo "1152 64" > $T; sleep 3
S 013_Bottom_Sheet_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 267" > $T; sleep 3
S 014_Bottom_Sheet_w01_MaterialButton
echo back > $T; sleep 2
echo "600 422" > $T; sleep 3
S 015_Bottom_Sheet_w02_MaterialButton
echo back > $T; sleep 2
echo "600 577" > $T; sleep 3
S 016_Bottom_Sheet_w03_MaterialButton
echo back > $T; sleep 2
echo "600 732" > $T; sleep 3
S 017_Bottom_Sheet_w04_MaterialButton
echo back > $T; sleep 2
echo "600 887" > $T; sleep 3
S 018_Bottom_Sheet_w05_MaterialButton
echo back > $T; sleep 2
echo "600 1042" > $T; sleep 3
S 019_Bottom_Sheet_w06_MaterialButton
echo back > $T; sleep 2
echo "456 1213" > $T; sleep 3
S 020_Bottom_Sheet_w07_MaterialButton
echo back > $T; sleep 2
echo "555 1213" > $T; sleep 3
S 021_Bottom_Sheet_w08_MaterialButton
echo back > $T; sleep 2
echo "651 1213" > $T; sleep 3
S 022_Bottom_Sheet_w09_MaterialButton
echo back > $T; sleep 2
echo "747 1213" > $T; sleep 3
S 023_Bottom_Sheet_w10_MaterialButton
echo back > $T; sleep 2
echo "600 1398" > $T; sleep 3
S 024_Bottom_Sheet_w11_MaterialSwitch
echo back > $T; sleep 2
echo "600 1526" > $T; sleep 3
S 025_Bottom_Sheet_w12_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Bottom_Sheet $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Buttons" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "682 771" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 026_Buttons_enter
echo "1152 64" > $T; sleep 3
S 027_Buttons_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 1176" > $T; sleep 3
S 028_Buttons_w01_MaterialCardView
echo back > $T; sleep 2
echo "600 1592" > $T; sleep 3
S 029_Buttons_w02_MaterialCardView
echo back > $T; sleep 2
echo "600 2008" > $T; sleep 3
S 030_Buttons_w03_MaterialCardView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Buttons $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Cards" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "68 1131" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 031_Cards_enter
echo "1152 64" > $T; sleep 3
S 032_Cards_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "110 254" > $T; sleep 3
S 033_Cards_w01_MaterialCheckBox
echo back > $T; sleep 2
echo "305 254" > $T; sleep 3
S 034_Cards_w02_MaterialCheckBox
echo back > $T; sleep 2
echo "500 254" > $T; sleep 3
S 035_Cards_w03_MaterialCheckBox
echo back > $T; sleep 2
echo "600 558" > $T; sleep 3
S 036_Cards_w04_MaterialCheckBox
echo back > $T; sleep 2
echo "600 951" > $T; sleep 3
S 037_Cards_w05_MaterialCheckBox
echo back > $T; sleep 2
echo "608 1047" > $T; sleep 3
S 038_Cards_w06_MaterialCheckBox
echo back > $T; sleep 2
echo "608 1143" > $T; sleep 3
S 039_Cards_w07_MaterialCheckBox
echo back > $T; sleep 2
echo "608 1239" > $T; sleep 3
S 040_Cards_w08_MaterialCheckBox
echo back > $T; sleep 2
echo "64 1407" > $T; sleep 3
S 041_Cards_w09_MaterialCheckBox
echo back > $T; sleep 2
echo "600 1537" > $T; sleep 3
S 042_Cards_w10_MaterialCheckBox
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Cards $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Carousel" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "687 1131" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 043_Carousel_enter
echo "1152 64" > $T; sleep 3
S 044_Carousel_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "730 330" > $T; sleep 3
S 045_Carousel_w01_Chip
echo back > $T; sleep 2
echo "728 458" > $T; sleep 3
S 046_Carousel_w02_Chip
echo back > $T; sleep 2
echo "706 586" > $T; sleep 3
S 047_Carousel_w03_Chip
echo back > $T; sleep 2
echo "692 714" > $T; sleep 3
S 048_Carousel_w04_Chip
echo back > $T; sleep 2
echo "692 842" > $T; sleep 3
S 049_Carousel_w05_Chip
echo back > $T; sleep 2
echo "710 970" > $T; sleep 3
S 050_Carousel_w06_Chip
echo back > $T; sleep 2
echo "710 1098" > $T; sleep 3
S 051_Carousel_w07_Chip
echo back > $T; sleep 2
echo "692 1232" > $T; sleep 3
S 052_Carousel_w08_Chip
echo back > $T; sleep 2
echo "692 1360" > $T; sleep 3
S 053_Carousel_w09_Chip
echo back > $T; sleep 2
echo "692 1488" > $T; sleep 3
S 054_Carousel_w10_Chip
echo back > $T; sleep 2
echo "599 1638" > $T; sleep 3
S 055_Carousel_w11_MaterialSwitch
echo back > $T; sleep 2
echo "599 1766" > $T; sleep 3
S 056_Carousel_w12_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Carousel $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Checkbox" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "95 1491" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 057_Checkbox_enter
echo "1152 64" > $T; sleep 3
S 058_Checkbox_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "599 192" > $T; sleep 3
S 059_Checkbox_w01_MaterialButton
echo back > $T; sleep 2
echo "154 363" > $T; sleep 3
S 060_Checkbox_w02_MaterialRadioButton
echo back > $T; sleep 2
echo "415 363" > $T; sleep 3
S 061_Checkbox_w03_MaterialRadioButton
echo back > $T; sleep 2
echo "121 550" > $T; sleep 3
S 062_Checkbox_w04_MaterialRadioButton
echo back > $T; sleep 2
echo "114 646" > $T; sleep 3
S 063_Checkbox_w05_MaterialRadioButton
echo back > $T; sleep 2
echo "144 742" > $T; sleep 3
S 064_Checkbox_w06_MaterialRadioButton
echo back > $T; sleep 2
echo "123 838" > $T; sleep 3
S 065_Checkbox_w07_MaterialRadioButton
echo back > $T; sleep 2
echo "121 1025" > $T; sleep 3
S 066_Checkbox_w08_MaterialRadioButton
echo back > $T; sleep 2
echo "330 1025" > $T; sleep 3
S 067_Checkbox_w09_MaterialRadioButton
echo back > $T; sleep 2
echo "586 1025" > $T; sleep 3
S 068_Checkbox_w10_MaterialRadioButton
echo back > $T; sleep 2
echo "121 1212" > $T; sleep 3
S 069_Checkbox_w11_MaterialRadioButton
echo back > $T; sleep 2
echo "172 1308" > $T; sleep 3
S 070_Checkbox_w12_MaterialRadioButton
echo back > $T; sleep 2
echo "142 1404" > $T; sleep 3
S 071_Checkbox_w13_MaterialRadioButton
echo back > $T; sleep 2
echo "165 1500" > $T; sleep 3
S 072_Checkbox_w14_MaterialRadioButton
echo back > $T; sleep 2
echo "177 1596" > $T; sleep 3
S 073_Checkbox_w15_MaterialRadioButton
echo back > $T; sleep 2
echo "121 1783" > $T; sleep 3
S 074_Checkbox_w16_MaterialRadioButton
echo back > $T; sleep 2
echo "318 1783" > $T; sleep 3
S 075_Checkbox_w17_MaterialRadioButton
echo back > $T; sleep 2
echo "590 1783" > $T; sleep 3
S 076_Checkbox_w18_MaterialRadioButton
echo back > $T; sleep 2
echo "121 1970" > $T; sleep 3
S 077_Checkbox_w19_MaterialRadioButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Checkbox $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Chips" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "667 1491" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 078_Chips_enter
echo "1152 64" > $T; sleep 3
S 079_Chips_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 192" > $T; sleep 3
S 080_Chips_w01_MaterialButton
echo back > $T; sleep 2
echo "600 288" > $T; sleep 3
S 081_Chips_w02_MaterialButton
echo back > $T; sleep 2
echo "600 384" > $T; sleep 3
S 082_Chips_w03_MaterialButton
echo back > $T; sleep 2
echo "600 480" > $T; sleep 3
S 083_Chips_w04_MaterialButton
echo back > $T; sleep 2
echo "600 576" > $T; sleep 3
S 084_Chips_w05_MaterialButton
echo back > $T; sleep 2
echo "600 672" > $T; sleep 3
S 085_Chips_w06_MaterialButton
echo back > $T; sleep 2
echo "600 768" > $T; sleep 3
S 086_Chips_w07_MaterialButton
echo back > $T; sleep 2
echo "600 864" > $T; sleep 3
S 087_Chips_w08_MaterialButton
echo back > $T; sleep 2
echo "600 960" > $T; sleep 3
S 088_Chips_w09_MaterialButton
echo back > $T; sleep 2
echo "600 1056" > $T; sleep 3
S 089_Chips_w10_MaterialButton
echo back > $T; sleep 2
echo "600 1152" > $T; sleep 3
S 090_Chips_w11_MaterialButton
echo back > $T; sleep 2
echo "600 1248" > $T; sleep 3
S 091_Chips_w12_MaterialButton
echo back > $T; sleep 2
echo "600 1344" > $T; sleep 3
S 092_Chips_w13_MaterialButton
echo back > $T; sleep 2
echo "600 1440" > $T; sleep 3
S 093_Chips_w14_MaterialButton
echo back > $T; sleep 2
echo "600 1536" > $T; sleep 3
S 094_Chips_w15_MaterialButton
echo back > $T; sleep 2
echo "600 1632" > $T; sleep 3
S 095_Chips_w16_MaterialButton
echo back > $T; sleep 2
echo "600 1728" > $T; sleep 3
S 096_Chips_w17_MaterialButton
echo back > $T; sleep 2
echo "600 1824" > $T; sleep 3
S 097_Chips_w18_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Chips $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Color" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "66 1201" > $T; sleep 7
echo "600 571" > $T; sleep 8
S 098_Color_enter
echo "1152 64" > $T; sleep 3
S 099_Color_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "445 1810" > $T; sleep 3
S 100_Color_w01_MaterialButton
echo back > $T; sleep 2
echo "749 1810" > $T; sleep 3
S 101_Color_w02_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Color $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Date_Picker" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "705 1201" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 102_Date_Picker_enter
echo "1152 64" > $T; sleep 3
S 103_Date_Picker_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "408 278" > $T; sleep 3
S 104_Date_Picker_w01_FloatingActionButton
echo back > $T; sleep 2
echo "560 278" > $T; sleep 3
S 105_Date_Picker_w02_FloatingActionButton
echo back > $T; sleep 2
echo "752 278" > $T; sleep 3
S 106_Date_Picker_w03_FloatingActionButton
echo back > $T; sleep 2
echo "408 588" > $T; sleep 3
S 107_Date_Picker_w04_FloatingActionButton
echo back > $T; sleep 2
echo "560 588" > $T; sleep 3
S 108_Date_Picker_w05_FloatingActionButton
echo back > $T; sleep 2
echo "752 588" > $T; sleep 3
S 109_Date_Picker_w06_FloatingActionButton
echo back > $T; sleep 2
echo "408 898" > $T; sleep 3
S 110_Date_Picker_w07_FloatingActionButton
echo back > $T; sleep 2
echo "560 898" > $T; sleep 3
S 111_Date_Picker_w08_FloatingActionButton
echo back > $T; sleep 2
echo "752 898" > $T; sleep 3
S 112_Date_Picker_w09_FloatingActionButton
echo back > $T; sleep 2
echo "408 1208" > $T; sleep 3
S 113_Date_Picker_w10_FloatingActionButton
echo back > $T; sleep 2
echo "560 1208" > $T; sleep 3
S 114_Date_Picker_w11_FloatingActionButton
echo back > $T; sleep 2
echo "752 1208" > $T; sleep 3
S 115_Date_Picker_w12_FloatingActionButton
echo back > $T; sleep 2
echo "408 1518" > $T; sleep 3
S 116_Date_Picker_w13_FloatingActionButton
echo back > $T; sleep 2
echo "560 1518" > $T; sleep 3
S 117_Date_Picker_w14_FloatingActionButton
echo back > $T; sleep 2
echo "752 1518" > $T; sleep 3
S 118_Date_Picker_w15_FloatingActionButton
echo back > $T; sleep 2
echo "464 1872" > $T; sleep 3
S 119_Date_Picker_w16_MaterialButton
echo back > $T; sleep 2
echo "723 1872" > $T; sleep 3
S 120_Date_Picker_w17_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Date_Picker $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Dialogs" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "80 1561" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 121_Dialogs_enter
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Dialogs $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Divider" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "677 1561" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 122_Divider_enter
echo "1152 64" > $T; sleep 3
S 123_Divider_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Divider $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Elevation_and_Shadow" >> /data/local/tmp/soak616/soak.log
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
S 124_Elevation_and_Shadow_enter
echo "1152 64" > $T; sleep 3
S 125_Elevation_and_Shadow_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "80 204" > $T; sleep 3
S 126_Elevation_and_Shadow_w01_NavigationRailItemView
echo back > $T; sleep 2
echo "80 325" > $T; sleep 3
S 127_Elevation_and_Shadow_w02_NavigationRailItemView
echo back > $T; sleep 2
echo "80 447" > $T; sleep 3
S 128_Elevation_and_Shadow_w03_NavigationRailItemView
echo back > $T; sleep 2
echo "679 230" > $T; sleep 3
S 129_Elevation_and_Shadow_w04_MaterialButton
echo back > $T; sleep 2
echo "679 326" > $T; sleep 3
S 130_Elevation_and_Shadow_w05_MaterialButton
echo back > $T; sleep 2
echo "679 422" > $T; sleep 3
S 131_Elevation_and_Shadow_w06_MaterialButton
echo back > $T; sleep 2
echo "768 518" > $T; sleep 3
S 132_Elevation_and_Shadow_w07_AppCompatSpinner
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Elevation_and_Shadow $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Floating_Action_Button" >> /data/local/tmp/soak616/soak.log
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
S 133_Floating_Action_Button_enter
echo "1152 64" > $T; sleep 3
S 134_Floating_Action_Button_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 428" > $T; sleep 3
S 135_Floating_Action_Button_w01_MaterialSwitch
echo back > $T; sleep 2
echo "502 575" > $T; sleep 3
echo wl616 > $X; sleep 3
S 136_Floating_Action_Button_w02_AppCompatEditText
echo back > $T; sleep 2
echo "1070 584" > $T; sleep 3
S 137_Floating_Action_Button_w03_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Floating_Action_Button $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Image_View" >> /data/local/tmp/soak616/soak.log
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
S 138_Image_View_enter
echo "600 421" > $T; sleep 3
S 139_Image_View_w00_ConstraintLayout
echo back > $T; sleep 2
echo "1120 421" > $T; sleep 3
S 140_Image_View_w01_MaterialButton
echo back > $T; sleep 2
echo "600 642" > $T; sleep 3
S 141_Image_View_w02_ConstraintLayout
echo back > $T; sleep 2
echo "1120 642" > $T; sleep 3
S 142_Image_View_w03_MaterialButton
echo back > $T; sleep 2
echo "1152 64" > $T; sleep 3
S 143_Image_View_w04_ActionMenuItemView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Image_View $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Menus" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "674 1631" > $T; sleep 7
echo "600 421" > $T; sleep 8
S 144_Menus_enter
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "done Menus $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Navigation_Bar_Bottom_Navigation" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "262 1341" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 145_Navigation_Bar_Bottom_Navigation_enter
echo "1152 64" > $T; sleep 3
S 146_Navigation_Bar_Bottom_Navigation_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 230" > $T; sleep 3
S 147_Navigation_Bar_Bottom_Navigation_w01_MaterialButton
echo back > $T; sleep 2
echo "600 326" > $T; sleep 3
S 148_Navigation_Bar_Bottom_Navigation_w02_MaterialButton
echo back > $T; sleep 2
echo "599 422" > $T; sleep 3
S 149_Navigation_Bar_Bottom_Navigation_w03_MaterialButton
echo back > $T; sleep 2
echo "688 518" > $T; sleep 3
S 150_Navigation_Bar_Bottom_Navigation_w04_AppCompatSpinner
echo back > $T; sleep 2
echo "262 1840" > $T; sleep 3
S 151_Navigation_Bar_Bottom_Navigation_w05_BottomNavigationItemView
echo back > $T; sleep 2
echo "599 1840" > $T; sleep 3
S 152_Navigation_Bar_Bottom_Navigation_w06_BottomNavigationItemView
echo back > $T; sleep 2
echo "936 1840" > $T; sleep 3
S 153_Navigation_Bar_Bottom_Navigation_w07_BottomNavigationItemView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Navigation_Bar_Bottom_Navigation $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Navigation_Drawer" >> /data/local/tmp/soak616/soak.log
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 500 600 1600" > $T; sleep 2
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "600 1400 600 700" > $T; sleep 3
echo "750 1341" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 154_Navigation_Drawer_enter
echo "1152 64" > $T; sleep 3
S 155_Navigation_Drawer_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "146 249" > $T; sleep 3
S 156_Navigation_Drawer_w01_MaterialSwitch
echo back > $T; sleep 2
echo "413 249" > $T; sleep 3
S 157_Navigation_Drawer_w02_MaterialSwitch
echo back > $T; sleep 2
echo "600 554" > $T; sleep 3
S 158_Navigation_Drawer_w03_MaterialSwitch
echo back > $T; sleep 2
echo "139 939" > $T; sleep 3
S 159_Navigation_Drawer_w04_SwitchMaterial
echo back > $T; sleep 2
echo "392 939" > $T; sleep 3
S 160_Navigation_Drawer_w05_SwitchMaterial
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Navigation_Drawer $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Navigation_Rail" >> /data/local/tmp/soak616/soak.log
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
S 161_Navigation_Rail_enter
echo "1152 64" > $T; sleep 3
S 162_Navigation_Rail_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 163_Navigation_Rail_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 164_Navigation_Rail_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 165_Navigation_Rail_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 166_Navigation_Rail_w04_TextInputEditText
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Navigation_Rail $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Progress_Indicator" >> /data/local/tmp/soak616/soak.log
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
S 167_Progress_Indicator_enter
echo "1152 64" > $T; sleep 3
S 168_Progress_Indicator_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 169_Progress_Indicator_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 170_Progress_Indicator_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 171_Progress_Indicator_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 172_Progress_Indicator_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 173_Progress_Indicator_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 174_Progress_Indicator_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 175_Progress_Indicator_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 176_Progress_Indicator_w08_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Progress_Indicator $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Radio_Button" >> /data/local/tmp/soak616/soak.log
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
S 177_Radio_Button_enter
echo "1152 64" > $T; sleep 3
S 178_Radio_Button_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 179_Radio_Button_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 180_Radio_Button_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 181_Radio_Button_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 182_Radio_Button_w04_TextInputEditText
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Radio_Button $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Search" >> /data/local/tmp/soak616/soak.log
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
S 183_Search_enter
echo "1152 64" > $T; sleep 3
S 184_Search_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 185_Search_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 186_Search_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 187_Search_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 188_Search_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 189_Search_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 190_Search_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 191_Search_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 192_Search_w08_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Search $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Shape_Theming" >> /data/local/tmp/soak616/soak.log
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
S 193_Shape_Theming_enter
echo "1152 64" > $T; sleep 3
S 194_Shape_Theming_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "400 272 800 272" > $T; sleep 3
S 195_Shape_Theming_w01_Slider
echo back > $T; sleep 2
echo "400 432 800 432" > $T; sleep 3
S 196_Shape_Theming_w02_RangeSlider
echo back > $T; sleep 2
echo "120 560" > $T; sleep 3
S 197_Shape_Theming_w03_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Shape_Theming $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Side_Sheet" >> /data/local/tmp/soak616/soak.log
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
echo "699 1121" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 198_Side_Sheet_enter
echo "1152 64" > $T; sleep 3
S 199_Side_Sheet_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "146 249" > $T; sleep 3
S 200_Side_Sheet_w01_MaterialSwitch
echo back > $T; sleep 2
echo "413 249" > $T; sleep 3
S 201_Side_Sheet_w02_MaterialSwitch
echo back > $T; sleep 2
echo "600 554" > $T; sleep 3
S 202_Side_Sheet_w03_MaterialSwitch
echo back > $T; sleep 2
echo "139 939" > $T; sleep 3
S 203_Side_Sheet_w04_SwitchMaterial
echo back > $T; sleep 2
echo "392 939" > $T; sleep 3
S 204_Side_Sheet_w05_SwitchMaterial
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Side_Sheet $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Slider" >> /data/local/tmp/soak616/soak.log
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
S 205_Slider_enter
echo "1152 64" > $T; sleep 3
S 206_Slider_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 207_Slider_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 208_Slider_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 209_Slider_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 210_Slider_w04_TextInputEditText
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Slider $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Switch" >> /data/local/tmp/soak616/soak.log
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
S 211_Switch_enter
echo "1152 64" > $T; sleep 3
S 212_Switch_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 213_Switch_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 214_Switch_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 215_Switch_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 216_Switch_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 217_Switch_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 218_Switch_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 219_Switch_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 220_Switch_w08_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Switch $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Tabs" >> /data/local/tmp/soak616/soak.log
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
S 221_Tabs_enter
echo "1152 64" > $T; sleep 3
S 222_Tabs_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "400 272 800 272" > $T; sleep 3
S 223_Tabs_w01_Slider
echo back > $T; sleep 2
echo "400 432 800 432" > $T; sleep 3
S 224_Tabs_w02_RangeSlider
echo back > $T; sleep 2
echo "120 560" > $T; sleep 3
S 225_Tabs_w03_MaterialButton
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Tabs $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Text_Field" >> /data/local/tmp/soak616/soak.log
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
echo "693 1191" > $T; sleep 7
echo "600 369" > $T; sleep 8
S 226_Text_Field_enter
echo "1152 64" > $T; sleep 3
S 227_Text_Field_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "146 249" > $T; sleep 3
S 228_Text_Field_w01_MaterialSwitch
echo back > $T; sleep 2
echo "413 249" > $T; sleep 3
S 229_Text_Field_w02_MaterialSwitch
echo back > $T; sleep 2
echo "600 554" > $T; sleep 3
S 230_Text_Field_w03_MaterialSwitch
echo back > $T; sleep 2
echo "139 939" > $T; sleep 3
S 231_Text_Field_w04_SwitchMaterial
echo back > $T; sleep 2
echo "392 939" > $T; sleep 3
S 232_Text_Field_w05_SwitchMaterial
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Text_Field $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Time_Picker" >> /data/local/tmp/soak616/soak.log
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
S 233_Time_Picker_enter
echo "1152 64" > $T; sleep 3
S 234_Time_Picker_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "600 234" > $T; sleep 3
echo wl616 > $X; sleep 3
S 235_Time_Picker_w01_TextInputEditText
echo back > $T; sleep 2
echo "600 405" > $T; sleep 3
echo wl616 > $X; sleep 3
S 236_Time_Picker_w02_TextInputEditText
echo back > $T; sleep 2
echo "600 565" > $T; sleep 3
echo wl616 > $X; sleep 3
S 237_Time_Picker_w03_TextInputEditText
echo back > $T; sleep 2
echo "600 731" > $T; sleep 3
echo wl616 > $X; sleep 3
S 238_Time_Picker_w04_TextInputEditText
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Time_Picker $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Top_App_Bar" >> /data/local/tmp/soak616/soak.log
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
S 239_Top_App_Bar_enter
echo "1152 64" > $T; sleep 3
S 240_Top_App_Bar_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "171 711" > $T; sleep 3
S 241_Top_App_Bar_w01_MaterialButton
echo back > $T; sleep 2
echo "426 267" > $T; sleep 3
S 242_Top_App_Bar_w02_MaterialButton
echo back > $T; sleep 2
echo "607 267" > $T; sleep 3
S 243_Top_App_Bar_w03_MaterialButton
echo back > $T; sleep 2
echo "781 267" > $T; sleep 3
S 244_Top_App_Bar_w04_MaterialButton
echo back > $T; sleep 2
echo "404 487" > $T; sleep 3
S 245_Top_App_Bar_w05_MaterialButton
echo back > $T; sleep 2
echo "585 487" > $T; sleep 3
S 246_Top_App_Bar_w06_MaterialButton
echo back > $T; sleep 2
echo "781 487" > $T; sleep 3
S 247_Top_App_Bar_w07_MaterialButton
echo back > $T; sleep 2
echo "600 615" > $T; sleep 3
S 248_Top_App_Bar_w08_MaterialSwitch
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Top_App_Bar $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Transition" >> /data/local/tmp/soak616/soak.log
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
S 249_Transition_enter
echo "600 421" > $T; sleep 3
S 250_Transition_w00_ConstraintLayout
echo back > $T; sleep 2
echo "1120 421" > $T; sleep 3
S 251_Transition_w01_MaterialButton
echo back > $T; sleep 2
echo "600 642" > $T; sleep 3
S 252_Transition_w02_ConstraintLayout
echo back > $T; sleep 2
echo "1120 642" > $T; sleep 3
S 253_Transition_w03_MaterialButton
echo back > $T; sleep 2
echo "600 786" > $T; sleep 3
S 254_Transition_w04_ConstraintLayout
echo back > $T; sleep 2
echo "1120 786" > $T; sleep 3
S 255_Transition_w05_MaterialButton
echo back > $T; sleep 2
echo "600 930" > $T; sleep 3
S 256_Transition_w06_ConstraintLayout
echo back > $T; sleep 2
echo "1120 930" > $T; sleep 3
S 257_Transition_w07_MaterialButton
echo back > $T; sleep 2
echo "600 1074" > $T; sleep 3
S 258_Transition_w08_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1074" > $T; sleep 3
S 259_Transition_w09_MaterialButton
echo back > $T; sleep 2
echo "600 1218" > $T; sleep 3
S 260_Transition_w10_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1218" > $T; sleep 3
S 261_Transition_w11_MaterialButton
echo back > $T; sleep 2
echo "600 1362" > $T; sleep 3
S 262_Transition_w12_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1362" > $T; sleep 3
S 263_Transition_w13_MaterialButton
echo back > $T; sleep 2
echo "600 1506" > $T; sleep 3
S 264_Transition_w14_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1506" > $T; sleep 3
S 265_Transition_w15_MaterialButton
echo back > $T; sleep 2
echo "600 1650" > $T; sleep 3
S 266_Transition_w16_ConstraintLayout
echo back > $T; sleep 2
echo "1120 1650" > $T; sleep 3
S 267_Transition_w17_MaterialButton
echo back > $T; sleep 2
echo "1152 64" > $T; sleep 3
S 268_Transition_w18_ActionMenuItemView
echo back > $T; sleep 2
sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1
echo "done Transition $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo "== Typography" >> /data/local/tmp/soak616/soak.log
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
echo "707 1719" > $T; sleep 7
echo "600 471" > $T; sleep 8
S 269_Typography_enter
echo "1152 64" > $T; sleep 3
S 270_Typography_w00_ActionMenuItemView
echo back > $T; sleep 2
echo "1136 176" > $T; sleep 3
S 271_Typography_w01_AppCompatImageView
echo back > $T; sleep 2
echo "1136 392" > $T; sleep 3
S 272_Typography_w02_AppCompatImageView
echo back > $T; sleep 2
echo "1136 577" > $T; sleep 3
S 273_Typography_w03_AppCompatImageView
echo back > $T; sleep 2
echo "1136 738" > $T; sleep 3
S 274_Typography_w04_AppCompatImageView
echo back > $T; sleep 2
echo "1136 888" > $T; sleep 3
S 275_Typography_w05_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1028" > $T; sleep 3
S 276_Typography_w06_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1157" > $T; sleep 3
S 277_Typography_w07_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1280" > $T; sleep 3
S 278_Typography_w08_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1387" > $T; sleep 3
S 279_Typography_w09_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1489" > $T; sleep 3
S 280_Typography_w10_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1596" > $T; sleep 3
S 281_Typography_w11_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1698" > $T; sleep 3
S 282_Typography_w12_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1795" > $T; sleep 3
S 283_Typography_w13_AppCompatImageView
echo back > $T; sleep 2
echo "1136 1897" > $T; sleep 3
S 284_Typography_w14_AppCompatImageView
echo back > $T; sleep 2
echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4
echo "done Typography $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log
echo SOAK-END $(date) >> /data/local/tmp/soak616/soak.log
