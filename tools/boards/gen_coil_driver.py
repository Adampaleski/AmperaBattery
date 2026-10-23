#!/usr/bin/env python3
"""Generator for docs/boards/coil_driver.layout.json (PROPOSED, Gilfoyle Build 20/21/24). Rerun after editing; the perfboard page edits the JSON directly too."""
import json, pathlib
L=lambda r,c: (chr(65+r) if r<26 else "A"+chr(65+r-26))+str(c)
chs=[("B+","S01","A:11","EV200 B+ coil","V12_BLK1",11),("PRE","S02","A:12","AEV14012 precharge coil","V12_BLK2",19),("B−","S03","A:21","EV200 B− coil","V12_BLK1",27)]
parts=[];wires=[];nets={}
def W(net,side,awg,pts,note=None):
    w={"id":f"w{len(wires)+1}","net":net,"side":side,"awg":awg,"pts":[L(*p) for p in pts]}
    if note: w["note"]=note
    wires.append(w)
H="18 AWG solid (coil path)"; S="22 AWG (signal)"
H16="doubled 20 AWG solid bus (coil return on board; DECIDED Sparky 2026-10-23 06:57 #5: <1 A continuous, 7.6 A for 130 ms)"
NC=[]
nets["GND"]={"members":[],"desc":"STAR ground at V22: all FET sources, the 10k pull-downs, Teensy signal GND (J-SIG.1, in the gate cable) and the coil return (on-board doubled 20 AWG solid bus -> J-GND.2 5.08 mm screw terminal rated ≥10 A -> TB-GND 16 AWG off-board, PINMAP G08) meet there only. DECIDED room 2026-10-23 06:11 + chief/Sparky 06:57 #5: TB-GND is the LV star; this board's only lead there is the coil return G08; no Teensy GND wire at TB-GND (G04 dropped)."}
for ch,sig,pin,coil,feed,C in chs:
    q,rg,rp,j=f"Q-{ch}",f"RG-{ch}",f"RP-{ch}",f"J-{ch}"
    parts+= [{"ref":j,"part":"cd_term","fp":"TB2_508","at":L(1,C-1),"rot":0,"pin_names":{"1":"NC (spare) — coil + is NOT on this board (low side only, DECIDED room 2026-10-23 06:11)","2":f"COIL− -> {coil} − ({sig})"}},
             {"ref":q,"part":"cd_fet","fp":"TO220","at":L(8,C),"rot":0,"value":"IRLB8721PbF"},
             {"ref":rg,"part":"cd_rgate","fp":"AXIAL400","at":L(11,C),"rot":90,"value":"100 Ω"},
             {"ref":rp,"part":"cd_rpull","fp":"AXIAL400","at":L(10,C+2),"rot":90,"value":"10 kΩ"}]
    NC.append(f"{j}.1")
    nets[f"COILN_{ch}"]={"members":[f"{j}.2",f"{q}.D"],"desc":f"{coil} − to {q} drain ({sig})"}
    nets[f"GATE_{ch}"]={"members":[f"{rg}.1",f"{q}.G",f"{rp}.2"],"desc":f"{q} gate: 100 Ω from {sig}, 10k to source"}
    nets[sig]={"members":[f"{rg}.2"],"desc":f"Teensy {pin} -> 100 Ω ({sig})"}
    nets["GND"]["members"]+= [f"{q}.S",f"{rp}.1"]
    W(f"COILN_{ch}","top",H,[(8,C+1),(1,C+1)])
    W(f"GATE_{ch}","top",S,[(8,C),(11,C)])
    W(f"GATE_{ch}","top",S,[(14,C+2),(14,C+1),(11,C+1),(11,C)])
    W("GND","top",H16,[(8,C+2),(10,C+2)])
star=(21,22)
W("GND","top",H16,[(10,13),(10,14),(21,14),star],"Q-B+ source -> star (coil return)")
W("GND","top",H16,[(10,21),(10,22),star],"Q-PRE source -> star (coil return)")
W("GND","top",H16,[(10,29),(10,30),(21,30),star],"Q-B− source -> star (coil return)")
parts.append({"ref":"J-SIG","part":"cd_term","fp":"TB4_508","at":L(24,10),"rot":180,"pin_names":{"1":"Teensy A signal GND (SGND, same cable as the gates; DECIDED 06:11)","2":"S03 Teensy A:21 (Q-B−)","3":"S02 Teensy A:12 (Q-PRE)","4":"S01 Teensy A:11 (Q-B+)"}})
parts.append({"ref":"J-GND","part":"cd_term","fp":"TB2_508","at":L(24,24),"rot":180,"pin_names":{"1":"NC (spare)","2":"coil return IN -> TB-GND only: 16 AWG off-board wire G08 in a 5.08 mm screw terminal rated ≥10 A (DECIDED Sparky 06:57 #5; terminal model UNVERIFIED)"}})
nets["S01"]["members"].append("J-SIG.4");nets["S02"]["members"].append("J-SIG.3");nets["S03"]["members"].append("J-SIG.2")
nets["GND"]["members"]+=["J-SIG.1","J-GND.2"]
W("S01","bot",S,[(24,4),(15,4),(15,11)])
W("S02","bot",S,[(24,6),(16,6),(16,19),(15,19)])
W("S03","bot",S,[(24,8),(17,8),(17,27),(15,27)])
W("GND","bot",S,[(24,10),(23,10),(23,20),(22,21),star],"Teensy signal GND (J-SIG.1) joins at the star only")
W("GND","bot",H16,[(24,22),star],"the ONE coil return to TB-GND: doubled 20 AWG on board, 16 AWG off-board from J-GND.2 (G08)")
lay={"schema":"teryx-perfboard/1","board":"coil_driver","title":"Contactor coil-driver board — 3 ch low-side (Q-B+ / Q-PRE / Q-B−)",
 "status":"PROPOSED","assembly":"coil_driver","board_part":"cd_board",
 "grid":{"pitch_mm":2.54,"rows":26,"cols":36},"mounts":["A1","A36","Z1","Z36"],"star":L(*star),
 "notes":[
  "PROPOSED (Gilfoyle 2026-10-23, Build 20; Build 21 applies the room decisions of 06:11; Build 24 applies chief + Sparky 06:57). Edit in tools/perfboard.html, Export JSON, commit to docs/boards/coil_driver.layout.json.",
  "Board: plated-through, double-sided perfboard (cd_board; Sparky 05:35). Solid wires = top side, dashed = bottom side. A wire is soldered at every hole it bends at (each vertex), nowhere else.",
  "Low side only (DECIDED room: Sparky + chief, 2026-10-23 06:11): the board switches coil − only. Coil + is fed off-board from F-COIL through the e-stop NC blocks (E01–E03, guide step 10); J-12V is deleted and pin 1 of J-B+/J-PRE/J-B− is an unused spare (nc).",
  "Coil return (DECIDED Sparky 2026-10-23 06:57 #5): FET sources -> star -> J-GND.2 on the board = DOUBLED 20 AWG solid bus (<1 A continuous, 7.6 A for 130 ms). 16 AWG (≈1.29 mm) will not fit the ≈1 mm perf holes, so the 16 AWG off-board return (PINMAP G08 -> TB-GND) lands in J-GND, a 5.08 mm screw terminal rated ≥10 A. Drains and coil − leads 18–20 AWG solid (Sparky 05:35). Gate/signal wires 22 AWG (PROPOSED).",
  "STAR ground at hole V22: the three FET sources, the 10k pull-downs, Teensy signal GND (J-SIG.1) and the coil return (J-GND.2) land there and nowhere else.",
  "Grounding (DECIDED room 06:11): J-SIG.1 = Teensy signal ground, in the same cable as the three gate signals. this board's ONLY lead to TB-GND is the coil return G08 (TB-GND = LV star: G03, G07, G08 + one bms_master link, chief 06:57 #5); there is no Teensy GND wire at TB-GND (PINMAP G04 dropped).",
  "Gate: Teensy pin -> 100 Ω -> G; 10k G-to-S on the FET side of the 100 Ω (PINMAP S01–S03; chief review #10).",
  "FETs stand upright, pins G-D-S left to right (IRLB8721PbF datasheet p.1). No heatsink (Sparky 05:35, guide check #6).",
  "Logic/signal terminals on the bottom edge, coil − terminals on the top edge (guide §2, Sparky 05:35/05:36).",
  "Strain relief: tie every off-board lead down next to its terminal; never let a solder joint hold a wire. 4× M3 standoffs at the corner holes; sealed box (Sparky 05:35).",
  "Flyback: D-PRE 1N4007 is OFF-BOARD, across the AEV14012 coil terminals (DECIDED Sparky 05:41). No diode on either EV200: built-in economizer (SOURCED, TE datasheet)."],
 "source":["docs/PINMAP.yaml S01–S03, E01–E03, TB-GND, G08, G04 (dropped)","bms_master_answers.md: chief + Sparky 2026-10-23 06:57 CT (DECIDED: 5.08 mm ≥10 A J-GND, doubled 20 AWG bus, cathode to coil +)","Room decision 2026-10-23 06:11 CT (Sparky + chief): low side only, G04 dropped, coil return 16 AWG+","docs/guided_install/coil_driver.md","Sparky chat 2026-10-23 05:31/05:35/05:36/05:41 CT","IRLB8721PbF datasheet (cutsheet:irlb8721)"],
 "parts":parts,"wires":wires,"nets":nets,"nc":["J-GND.1"]+NC,
 "offboard":[{"ref":"D-PRE","part":"cd_flyback","value":"1N4007","near":"J-PRE","short":"at the AEV14012 coil, cathode to coil +","text":"D-PRE 1N4007 OFF-BOARD at the AEV14012 coil terminals: mounted AT THE COIL, cathode to coil + (E03 feed; cathode = banded end), anode to coil − (S02, J-PRE.2). DECIDED Sparky 05:41, confirmed room 06:11; wording 'cathode to coil +' DECIDED chief 06:57 #6. No diode on the EV200 coils."}]}
import sys; sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent)); import install_steps  # Build 25: Install-mode steps from docs/guided_install/coil_driver.md §3
install_steps.attach(lay,install_steps.COIL_DRIVER,json.loads((pathlib.Path(__file__).resolve().parents[2]/"docs/boards/footprints.json").read_text()))
open(pathlib.Path(__file__).resolve().parents[2]/"docs/boards/coil_driver.layout.json","w").write(json.dumps(lay,indent=1,ensure_ascii=False)+"\n")
print(len(parts),len(wires),len(nets))
