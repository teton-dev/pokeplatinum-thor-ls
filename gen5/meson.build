# Gen 5 battles (teton-dev fork): Black 2 and White 2's battle code from pokebw2, compiled for the port.
# docs/thor/dev/gen5-battles.md has the plan. Built when -Dpokebw2=<path of a pokebw2 clone> is set (fork only: pokebw2
# is GPLv3). Milestone 1: compiled, not linked into the game yet.

# A path relative to this repository, such as ../pokebw2, or an absolute one
pokebw2 = meson.project_source_root() / get_option('pokebw2')
if not fs.is_dir(pokebw2 / 'src' / 'ov167')
    error('-Dpokebw2=' + pokebw2 + ' is not a pokebw2 clone')
endif

# pokebw2's tools need Python 3.11
gen5_python = find_program('python3', 'python', version: '>=3.11')

# pokebw2's constant and text ID headers, generated from its lists as its own build does
gen5_headers = custom_target('gen5_headers',
    # A header, so that every compile waits for it
    output: 'gen5_headers_stamp.h',
    command: [gen5_python, files('tools/gen_headers.py'), pokebw2, meson.current_build_dir() / 'generated', '@OUTPUT@'],
    build_always_stale: true,
)

# pokebw2's headers with the port's changes (patches/include): patched copies, ahead of pokebw2's on the quote path
gen5_header_patches = custom_target('gen5_header_patches',
    output: 'gen5_header_patches_stamp.h',
    command: [gen5_python, files('tools/apply_header_patches.py'), pokebw2, meson.current_source_dir() / 'patches',
              meson.current_build_dir() / 'include', '@OUTPUT@'],
    build_always_stale: true,
)

# pokebw2's files, unchanged
gen5_sources = [
    # The battle engine and the battle view's core (overlay 167)
    'src/ov167/ability_handlers.c',
    'src/ov167/battle_action.c',
    'src/ov167/battle_condition.c',
    'src/ov167/battle_event.c',
    'src/ov167/battle_overlay.c',
    'src/ov167/btl_adapter.c',
    'src/ov167/btl_calc.c',
    'src/ov167/btl_client.c',
    'src/ov167/btl_field.c',
    'src/ov167/btl_handler_work.c',
    'src/ov167/btl_main.c',
    'src/ov167/btl_net.c',
    'src/ov167/btl_pokeparam.c',
    'src/ov167/btl_rec.c',
    'src/ov167/btl_server.c',
    'src/ov167/btl_server_cmd.c',
    'src/ov167/btl_server_flow_sub.c',
    'src/ov167/btl_string.c',
    'src/ov167/btlv_scu.c',
    'src/ov167/handler_common.c',
    'src/ov167/item_handlers.c',
    'src/ov167/poke_type_pair.c',
    'src/ov167/pokewood_cutin.c',
    # The battle view: arena, camera, sprites, gauges, move animations, touch input (overlay 168)
    'src/ov168/btlv_b_gauge.c',
    'src/ov168/btlv_bg.c',
    'src/ov168/btlv_camera.c',
    'src/ov168/btlv_clact.c',
    'src/ov168/btlv_effect.c',
    'src/ov168/btlv_field.c',
    'src/ov168/btlv_mcss.c',
    'src/ov168/btlv_stage.c',
    'src/ov168/btlv_timer.c',
    # The battle's tables and field, side and position handlers (overlay 169)
    'src/ov169/btl_faintrec.c',
    'src/ov169/btl_monset.c',
    'src/ov169/btl_move_rec.c',
    'src/ov169/btl_poke_select.c',
    'src/ov169/btl_pospoke.c',
    'src/ov169/btl_tables.c',
    'src/ov169/btlv_scd.c',
    'src/ov169/condition_effects.c',
    'src/ov169/field_handlers.c',
    # The trainer AI (overlay 170)
    'src/ov170/tr_ai.c',
    # The battle screens' shared cursor and windows (overlay 285)
    'src/ov285/b_app_tool.c',
    # The battle bag (overlay 286)
    'src/ov286/b_bag_anm.c',
    'src/ov286/b_bag_bmp.c',
    'src/ov286/b_bag_item.c',
    'src/ov286/b_bag_main.c',
    'src/ov286/b_bag_obj.c',
    'src/ov286/b_bag_ui.c',
    # The battle party list (overlay 287)
    'src/ov287/b_plist_anm.c',
    'src/ov287/b_plist_bmp.c',
    'src/ov287/b_plist_cursor.c',
    'src/ov287/b_plist_main.c',
    'src/ov287/b_plist_obj.c',
    # The scenario battles' screen (overlay 288)
    'src/ov288/b_scenario_graphic.c',
    'src/ov288/b_scenario_main.c',
    # The battle's choice screen (overlay 289)
    'src/ov289/b_choice_main.c',
    # Game Freak's library (ARM9 main)
    'src/gfl/areaman.c',
    'src/gfl/assert.c',
    'src/gfl/backup_card.c',
    'src/gfl/backup_system.c',
    'src/gfl/bg_sys.c',
    'src/gfl/bmp.c',
    'src/gfl/bmp_win.c',
    'src/gfl/calctool.c',
    'src/gfl/clact.c',
    'src/gfl/clact_itcm.c',
    'src/gfl/debug_print.c',
    'src/gfl/dma_vblank.c',
    'src/gfl/fade.c',
    'src/gfl/g3d_camera.c',
    'src/gfl/gf_standard.c',
    'src/gfl/gx_layers.c',
    'src/gfl/heap_dtcm.c',
    'src/gfl/heapsys.c',
    'src/gfl/msgdata.c',
    'src/gfl/net.c',
    'src/gfl/net_command.c',
    'src/gfl/net_handle.c',
    'src/gfl/net_irc_wireless.c',
    'src/gfl/net_lower_data.c',
    'src/gfl/net_queue.c',
    'src/gfl/net_ring_buff.c',
    'src/gfl/net_state.c',
    'src/gfl/net_system.c',
    'src/gfl/procsys.c',
    'src/gfl/rtc_cache.c',
    'src/gfl/savedata.c',
    'src/gfl/strbuf.c',
    'src/gfl/systemfont.c',
    'src/gfl/tcb.c',
    'src/gfl/tcbl.c',
    'src/gfl/textprint.c',
    'src/gfl/ui.c',
    'src/gfl/vman.c',
    'src/gfl/wm_icon.c',
    # The game's own systems the battle uses: Pokémon data, parties, text, save blocks (ARM9 main)
    'src/system/adventure_savedata.c',
    'src/system/app_menu_common.c',
    'src/system/area_data.c',
    'src/system/bgwinfrm.c',
    'src/system/bmp_winframe.c',
    'src/system/box_savedata.c',
    'src/system/btl_setup.c',
    'src/system/comm_player_support.c',
    'src/system/config.c',
    'src/system/country_region.c',
    'src/system/cursor_move.c',
    'src/system/event_work.c',
    'src/system/evolution_check.c',
    'src/system/fest_mission.c',
    'src/system/field_light_status.c',
    'src/system/file_util.c',
    'src/system/gf_font.c',
    'src/system/hp_gauge.c',
    'src/system/infowin.c',
    'src/system/join_avenue_savedata.c',
    'src/system/key_data_savedata.c',
    'src/system/key_info.c',
    'src/system/mail.c',
    'src/system/medal_box.c',
    'src/system/memory_link_savedata.c',
    'src/system/misc_savedata.c',
    'src/system/move_tool.c',
    'src/system/musical_savedata.c',
    'src/system/myitem_savedata.c',
    'src/system/mystatus.c',
    'src/system/palanm.c',
    'src/system/pass_power.c',
    'src/system/perapvoice.c',
    'src/system/player_volume_fader.c',
    'src/system/playtime.c',
    'src/system/pml_hm_check.c',
    'src/system/pml_item.c',
    'src/system/pml_species_names.c',
    'src/system/pms_data.c',
    'src/system/pms_word.c',
    'src/system/poke_friendship.c',
    'src/system/poke_icon.c',
    'src/system/poke_personal.c',
    'src/system/poke_recover.c',
    'src/system/poke_regulation.c',
    'src/system/pokedex_habitat_savedata.c',
    'src/system/pokedex_savedata.c',
    'src/system/pokegra.c',
    'src/system/pokeparty.c',
    'src/system/printsys.c',
    'src/system/record.c',
    'src/system/regulation.c',
    'src/system/resort_binary.c',
    'src/system/resort_work.c',
    'src/system/ringtone_sys.c',
    'src/system/rtc.c',
    'src/system/save_error.c',
    'src/system/shooter_item.c',
    'src/system/sin_wave_table.c',
    'src/system/str_tool.c',
    'src/system/survey_data.c',
    'src/system/survey_savedata.c',
    'src/system/text_speed.c',
    'src/system/tr_tool.c',
    'src/system/trainer_card_savedata.c',
    'src/system/trainer_data_savedata.c',
    'src/system/trainer_game_info_savedata.c',
    'src/system/union_view.c',
    'src/system/version.c',
    'src/system/wipe.c',
    'src/system/wipe_sub.c',
    'src/system/wipe_wnd.c',
    'src/system/wordset.c',
    'src/system/zonedata.c',
    # Nintendo's SPL particle library, as built into BW2
    'lib/spl/src/spl_anim.c',
    'lib/spl/src/spl_behavior.c',
    'lib/spl/src/spl_draw.c',
    'lib/spl/src/spl_emit.c',
    'lib/spl/src/spl_emitter.c',
    'lib/spl/src/spl_list.c',
    'lib/spl/src/spl_manager.c',
    'lib/spl/src/spl_random.c',
]

# pokebw2's files with the port's changes (patches/), each compiled from a patched copy
gen5_patched = [
    'src/gfl/arc_tool.c',             # An assembly function in C
    'src/ov167/btlv_core.c',          # The command selection's event, for the test scripts (runtime.c)
    'src/gfl/arc_util.c',             # Multi-cell files in the 64-bit layout (src/g2d.c)
    'src/gfl/heap.c',                 # A heap deleted with blocks left: listed, not the debugger's exit
    'src/gfl/g3d_system.c',           # NitroSystem's name for a G3D file's signature as a u32
    'src/gfl/key.c',                  # The keys the game read (runtime.c)
    'src/gfl/touchpanel.c',           # The touch the game read (runtime.c)
    'src/ov168/btlv_effvm.c',         # The SDK's names of a matrix's translation; particle work for 64 bits
    'src/system/mcss.c',              # NitroSystem's names of a node's position; texture coordinates as commands
    'src/system/gfl_use.c',           # GFLAppInit takes the port's heaps (runtime.c)
    'src/system/pm_voice.c',          # No NULL sound channel handles (a cry that never started)
    # 64 bits: values that can be pointers, as uptr
    'src/gfl/particle.c',             # Particle_Alloc's address arithmetic, and a check of the work's end
    'src/ov167/btl_server_flow.c',    # A handler's string through an event variable (src/pointers.c)
    'src/ov167/move_handlers.c',      # The handler's string back from its event variable
    'src/ov168/btlv_finger_cursor.c', # A pointer through an animation callback's u32
    'src/ov168/btlv_gauge.c',         # VRAM address arithmetic
    'src/ov168/btlv_input.c',         # VRAM address arithmetic
    'src/ov169/pos_handlers.c',       # A pointer as a BOOL
    'src/ov169/side_handlers.c',      # The handler's string back from its event variable
    'src/system/mcss_tool.c',         # A pointer through an animation callback's u32
    'src/system/vm.c',                # Return addresses on the VM's stack
    'src/system/poke_tool.c',         # PokeParty_SetParam, PML_PkmSetParam (nicknames, OT names, mail)
    'src/system/pml_met_data.c',
    'src/system/resonance_resort_data.c', # No Join Avenue: a catch records nothing there
]

# The port's own code in place of pokebw2's: where BW2's battle meets what the port has instead of BW2's field,
# save, network and DSi systems
gen5_port_sources = files(
    'src/field.c',       # Beacons, Festival Missions, network devices... (none)
    'src/pointers.c',    # Pointers through 32-bit values as handles (64 bits)
    'src/g2d.c',         # NitroSystem 2D files libntrsystem doesn't convert for 64 bits
    'src/files.c',       # BW2's archives from the extracted ROM, and the self-check (api/gen5.h)
    'src/game_data.c',   # GameData and SaveControl: milestone 4 answers them from Platinum's save
    'src/gfl_overlay.c', # src/gfl/gf_overlay.c: no overlays
    'src/net.c',         # src/system/net_err.c: no network errors
    'src/runtime.c',     # BW2's systems run in the port, and a battle on them (api/gen5.h)
    'src/sdk.c',         # What pokebw2's own SDK copies have and libntr lacks
    'src/sound.c',       # src/system/snd_sys.c: silent for now (Platinum holds the sound library)
)

gen5_files = []
foreach path : gen5_sources
    gen5_files += files(pokebw2 / path)
endforeach

gen5_patched_sources = []
foreach path : gen5_patched
    gen5_patched_sources += custom_target(path.replace('/', '_'),
        input: [pokebw2 / path, 'patches' / path + '.patch'],
        output: fs.name(path),
        command: [gen5_python, files('tools/apply_patch.py'), '@INPUT0@', '@INPUT1@', '@OUTPUT@'],
    )
endforeach

gen5_args = [
    '-std=gnu99',
    '-DBLACK2',
    # pokebw2's headers by quote only, so that its copies of MSL's string.h and the like don't hide the host's
    '-iquote', meson.current_source_dir() / 'include',
    '-iquote', meson.current_source_dir() / 'api',
    '-iquote', meson.current_build_dir() / 'include',
    '-iquote', meson.current_build_dir() / 'generated',
    '-iquote', pokebw2 / 'include',
    '-iquote', pokebw2 / 'lib/spl/include',
    # The declarations of BW2's Wi-Fi (DWC, NitroWiFi) and DSi (TwlSDK) libraries, for the types and calls in its
    # headers. The libraries aren't built: the port's src/ has what the battle calls
    '-iquote', pokebw2 / 'lib/dwc/include',
    '-iquote', pokebw2 / 'lib/nitrowifi/include',
    '-iquote', pokebw2 / 'lib/twl/include',
    '-include', 'gen5/port.h',
    # As for the game's code: the decomp type-puns and relies on wrapping signed arithmetic
    '-fwrapv',
    # BW2 was built with a signed char (-char signed), which arm64's default isn't
    '-fsigned-char',
    '-fno-strict-aliasing',
    # Android's SDL_main.h renames main to SDL_main, which clashes with system/main.h's void main(void). The game's
    # entry point isn't in here
    '-DSDL_MAIN_HANDLED',
    # Pointers passed as integers: libntr takes the emulated registers' addresses as u64, which is right. The
    # warnings are the list to check for pointers cut to 32 bits
    '-Wno-error=int-conversion',
]

foreach path : gen5_patched
    # A patched copy's own includes from its directory
    gen5_args += ['-iquote', pokebw2 / fs.parent(path)]
endforeach

gen5_battle = static_library('gen5_battle',
    sources: [
        gen5_files,
        gen5_patched_sources,
        gen5_port_sources,
        gen5_headers,
        gen5_header_patches,
    ],
    c_args: gen5_args,
    dependencies: [
        nitrosdk_dep,
        nitrosystem_dep,
    ],
    native: (build_target == 'linux' or build_target == 'win64'),
)

# BW2's symbols renamed to bw2_<name> (tools/prefix_symbols.py), in one object: what the game links
gen5_battle_prefixed = custom_target('gen5_battle_prefixed',
    input: gen5_battle,
    output: 'gen5_battle_bw2.o',
    build_by_default: true,
    command: [gen5_python, files('tools/prefix_symbols.py'), find_program('nm', 'llvm-nm'),
              find_program('objcopy', 'llvm-objcopy'), find_program('ar', 'llvm-ar'), '@INPUT@', '@OUTPUT@',
              meson.get_compiler('c', native: (build_target == 'linux' or build_target == 'win64')).cmd_array()],
)
