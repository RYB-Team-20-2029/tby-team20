# Purpose: read ryb_comms.json, generate each module's config header and build RYB Comms targets.
# Owning module: management (mgmt/).
#
#   ryb_comms_add_library(<target> MODULE <name> [JSON <file>] [BACKEND mmio|sim])
#       Static library of RYB Comms configured for one module.
#   ryb_add_module(<name>)
#       Executable from modules/<name>/src/**.c, linked against ryb_comms_<name>.

include_guard(GLOBAL)

get_filename_component(_ryb_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(RYB_COMMS_DIR "${_ryb_dir}" CACHE INTERNAL "RYB Comms sources")
get_filename_component(_ryb_dir "${RYB_COMMS_DIR}/.." ABSOLUTE)
set(RYB_INCLUDE_DIR "${_ryb_dir}" CACHE INTERNAL "Shared include root, so code writes ryb/<header>")
get_filename_component(_ryb_dir "${RYB_COMMS_DIR}/../.." ABSOLUTE)
set(RYB_REPO_ROOT "${_ryb_dir}" CACHE INTERNAL "Repository root")

# Module directory -> function prefix (docs/coding-standard.md §4), the address the protocol
# guideline assigns it, and the name of the value its appended packets carry.
macro(_ryb_tables)
    set(_RYB_PREFIX_heartbeat hb)
    set(_RYB_PREFIX_sound snd)
    set(_RYB_PREFIX_decision dec)
    set(_RYB_PREFIX_motor mot)
    set(_RYB_SPEC_ADDRESS_heartbeat 0)
    set(_RYB_SPEC_ADDRESS_sound 1)
    set(_RYB_SPEC_ADDRESS_decision 2)
    set(_RYB_SPEC_ADDRESS_motor 3)
    set(_RYB_VALUE_heartbeat bpm)
    set(_RYB_VALUE_sound volume)
    set(_RYB_VALUE_motor state)
endmacro()

# Read one JSON member, or stop with a message naming it.
function(_ryb_json out json what)
    string(JSON value ERROR_VARIABLE err GET "${json}" ${ARGN})
    if(err)
        message(FATAL_ERROR "ryb_comms.json: cannot read ${what}: ${err}")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(_ryb_json_type out json what)
    string(JSON value ERROR_VARIABLE err TYPE "${json}" ${ARGN})
    if(err)
        message(FATAL_ERROR "ryb_comms.json: ${what} is missing")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(_ryb_json_int out json what min max)
    _ryb_json(value "${json}" "${what}" ${ARGN})
    if(NOT value MATCHES "^-?[0-9]+$" OR value LESS min OR value GREATER max)
        message(FATAL_ERROR
            "ryb_comms.json: ${what} must be a whole number from ${min} to ${max}, not '${value}'")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(_ryb_json_bool out json what)
    _ryb_json_type(type "${json}" "${what}" ${ARGN})
    if(NOT type STREQUAL "BOOLEAN")
        message(FATAL_ERROR "ryb_comms.json: ${what} must be true or false")
    endif()
    _ryb_json(value "${json}" "${what}" ${ARGN})
    if(value)
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction()

function(_ryb_pin_bit out pins pin what)
    string(JSON bit ERROR_VARIABLE err GET "${pins}" "${pin}")
    if(err)
        message(FATAL_ERROR "ryb_comms.json: ${what} is '${pin}', which is not in the pins table")
    endif()
    if(NOT bit MATCHES "^[0-9]+$" OR bit GREATER 63)
        message(FATAL_ERROR
            "ryb_comms.json: pins.${pin} must be a GPIO bit from 0 to 63, not '${bit}'")
    endif()
    set(${out} "${bit}" PARENT_SCOPE)
endfunction()

function(_ryb_warnings target)
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

function(ryb_comms_add_library target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "MODULE;JSON;BACKEND" "")
    _ryb_tables()
    find_package(Threads REQUIRED)

    if(NOT arg_MODULE)
        message(FATAL_ERROR "ryb_comms_add_library(${target}): MODULE is required")
    endif()
    if(NOT arg_JSON)
        set(arg_JSON "${RYB_COMMS_DIR}/ryb_comms.json")
    endif()
    get_filename_component(arg_JSON "${arg_JSON}" ABSOLUTE)
    if(NOT EXISTS "${arg_JSON}")
        message(FATAL_ERROR "RYB Comms config ${arg_JSON} does not exist")
    endif()
    if(NOT arg_BACKEND)
        set(arg_BACKEND mmio)
    endif()
    if(NOT arg_BACKEND MATCHES "^(mmio|sim)$")
        message(FATAL_ERROR "RYB Comms GPIO backend must be mmio or sim, not '${arg_BACKEND}'")
    endif()

    # Editing the JSON re-runs configure on the next build.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_JSON}")
    file(READ "${arg_JSON}" json)
    get_filename_component(json_name "${arg_JSON}" NAME)

    # --- Settings every module shares ---------------------------------------------------------
    _ryb_json(base "${json}" "gpio.base_address" gpio base_address)
    if(NOT base MATCHES "^0[xX][0-9a-fA-F]+$")
        message(FATAL_ERROR
            "ryb_comms.json: gpio.base_address must be a hex string like \"0x41200000\", "
            "not '${base}'")
    endif()
    _ryb_json_int(settle "${json}" "gpio.direction_settle_us" 0 1000000 gpio direction_settle_us)
    _ryb_json_int(bit_period "${json}" "line.bit_period_us" 20 1000000 line bit_period_us)
    _ryb_json_int(idle_sleep "${json}" "line.idle_sleep_us" 0 1000000 line idle_sleep_us)
    # A start edge can arrive just as the loop goes to sleep, and nanosleep() overshot by up to
    # 74 us on the PYNQ-Z2. The receiver splits that window in half, so the whole window has to
    # fit in half a bit for its sample points to stay inside their bits.
    math(EXPR idle_window "${idle_sleep} + 80")
    math(EXPR half_bit "${bit_period} / 2")
    if(idle_sleep GREATER 0 AND idle_window GREATER half_bit)
        message(FATAL_ERROR "ryb_comms.json: line.idle_sleep_us ${idle_sleep} plus about 80 us of "
            "sleep overshoot is more than half of line.bit_period_us ${bit_period}, so start "
            "bits are seen too late; raise the bit period or set idle_sleep_us to 0")
    endif()
    _ryb_json(drive "${json}" "line.drive" line drive)
    if(drive STREQUAL "open_drain")
        set(open_drain 1)
    elseif(drive STREQUAL "push_pull")
        set(open_drain 0)
    else()
        message(FATAL_ERROR
            "ryb_comms.json: line.drive must be \"open_drain\" or \"push_pull\", not '${drive}'")
    endif()
    _ryb_json_bool(realtime "${json}" "thread.realtime" thread realtime)
    if(realtime AND idle_sleep EQUAL 0)
        message(WARNING "ryb_comms.json: line.idle_sleep_us 0 with thread.realtime: the bit loop "
            "spins at 100 % CPU between packets, and Linux RT throttling then freezes it for about "
            "50 ms every second (kernel.sched_rt_runtime_us)")
    endif()
    _ryb_json_int(priority "${json}" "thread.priority" 1 99 thread priority)
    _ryb_json_int(cpu "${json}" "thread.cpu" -1 63 thread cpu)

    # "backbone": a pin name is the same conductor on every board. "point_to_point": each link is
    # its own wire between two boards, so every board may send on the same pin name.
    string(JSON wiring ERROR_VARIABLE err GET "${json}" line wiring)
    if(err)
        set(wiring backbone)
    elseif(NOT wiring MATCHES "^(backbone|point_to_point)$")
        message(FATAL_ERROR
            "ryb_comms.json: line.wiring must be \"backbone\" or \"point_to_point\", not '${wiring}'")
    endif()

    # Optional defaults for <module>_monitor.
    set(RYB_CFG_MONITOR_PERIOD_MS 500)
    set(RYB_CFG_MONITOR_RANDOM 0)
    string(JSON monitor_type ERROR_VARIABLE err TYPE "${json}" monitor)
    if(NOT err AND monitor_type STREQUAL "OBJECT")
        string(JSON v ERROR_VARIABLE err GET "${json}" monitor send_period_ms)
        if(NOT err)
            _ryb_json_int(RYB_CFG_MONITOR_PERIOD_MS "${json}" "monitor.send_period_ms" 0 3600000
                monitor send_period_ms)
        endif()
        string(JSON v ERROR_VARIABLE err GET "${json}" monitor values)
        if(NOT err)
            if(v STREQUAL "random")
                set(RYB_CFG_MONITOR_RANDOM 1)
            elseif(NOT v STREQUAL "sweep")
                message(FATAL_ERROR
                    "ryb_comms.json: monitor.values must be \"random\" or \"sweep\", not '${v}'")
            endif()
        endif()
    endif()
    _ryb_json(pins "${json}" "pins" pins)

    # --- LCD wiring, optional: without it ryb_lcd_init() reports that there is no display ----
    set(lcd_bits "")
    string(JSON lcd_type ERROR_VARIABLE err TYPE "${json}" lcd)
    if(err OR lcd_type STREQUAL "NULL")
        set(RYB_CFG_HAS_LCD 0)
        foreach(role BL RST DC CS SCLK MOSI)
            set(RYB_CFG_LCD_${role} 0)
        endforeach()
    else()
        set(RYB_CFG_HAS_LCD 1)
        foreach(role bl rst dc cs sclk mosi)
            _ryb_json(lcd_pin "${json}" "lcd.${role}" lcd ${role})
            _ryb_pin_bit(lcd_bit "${pins}" "${lcd_pin}" "lcd.${role}")
            if(lcd_bit IN_LIST lcd_bits)
                message(FATAL_ERROR "ryb_comms.json: lcd.${role} ${lcd_pin} is already another LCD pin")
            endif()
            list(APPEND lcd_bits ${lcd_bit})
            string(TOUPPER ${role} role_macro)
            set(RYB_CFG_LCD_${role_macro} ${lcd_bit})
            set(lcd_role_of_${lcd_bit} ${role})
        endforeach()
    endif()

    # --- Module table: every address must be known to resolve "from" lists -------------------
    _ryb_json(modules "${json}" "modules" modules)
    string(JSON module_count LENGTH "${modules}")
    if(module_count EQUAL 0)
        message(FATAL_ERROR "ryb_comms.json: modules is empty")
    endif()
    set(module_names "")
    math(EXPR last "${module_count} - 1")
    foreach(i RANGE ${last})
        string(JSON name MEMBER "${modules}" ${i})
        if(NOT DEFINED _RYB_PREFIX_${name})
            message(FATAL_ERROR "ryb_comms.json: unknown module '${name}'; "
                "expected heartbeat, sound, decision or motor")
        endif()
        _ryb_json_int(address "${modules}" "modules.${name}.address" 0 3 ${name} address)
        if(DEFINED name_of_${address})
            message(FATAL_ERROR "ryb_comms.json: modules.${name}.address ${address} is already "
                "used by ${name_of_${address}}")
        endif()
        if(NOT address EQUAL "${_RYB_SPEC_ADDRESS_${name}}")
            message(WARNING "ryb_comms.json: modules.${name}.address is ${address}, but the "
                "protocol guideline assigns ${_RYB_SPEC_ADDRESS_${name}}. That only works if "
                "every board builds from this same file.")
        endif()
        set(address_of_${name} ${address})
        set(name_of_${address} ${name})
        list(APPEND module_names ${name})

        _ryb_json_type(any_tx_type "${modules}" "modules.${name}.tx" ${name} tx)
        if(any_tx_type STREQUAL "STRING" AND NOT open_drain AND wiring STREQUAL "backbone")
            string(JSON any_tx GET "${modules}" ${name} tx)
            if(DEFINED tx_owner_${any_tx})
                # Checked across all modules, because the boards share the backbone conductors.
                message(FATAL_ERROR "ryb_comms.json: ${name} and ${tx_owner_${any_tx}} both "
                    "transmit on ${any_tx}; with line.drive \"push_pull\" that is two drivers "
                    "on one lane, a short the moment they disagree")
            endif()
            set(tx_owner_${any_tx} ${name})
        endif()
    endforeach()

    if(NOT DEFINED address_of_decision)
        message(FATAL_ERROR "ryb_comms.json: no decision module; receivers need its address to "
            "tell command packets from appended ones")
    endif()
    set(command_address ${address_of_decision})

    # --- The module being built ---------------------------------------------------------------
    set(m "${arg_MODULE}")
    if(NOT DEFINED address_of_${m})
        message(FATAL_ERROR "ryb_comms.json has no module '${m}' (it has: ${module_names})")
    endif()
    set(address ${address_of_${m}})
    set(prefix ${_RYB_PREFIX_${m}})
    _ryb_json(self "${modules}" "modules.${m}" ${m})
    _ryb_json_bool(repeater "${self}" "modules.${m}.repeater" repeater)

    _ryb_json_type(tx_type "${self}" "modules.${m}.tx (use null for no outgoing link)" tx)
    if(tx_type STREQUAL "NULL")
        set(tx_bit -1)
        set(tx_desc "none")
    elseif(tx_type STREQUAL "STRING")
        _ryb_json(tx_pin "${self}" "modules.${m}.tx" tx)
        _ryb_pin_bit(tx_bit "${pins}" "${tx_pin}" "modules.${m}.tx")
        if(tx_bit IN_LIST lcd_bits)
            message(FATAL_ERROR "ryb_comms.json: modules.${m}.tx ${tx_pin} is also the LCD's "
                "${lcd_role_of_${tx_bit}} pin")
        endif()
        set(tx_desc "${tx_pin}")
    else()
        message(FATAL_ERROR "ryb_comms.json: modules.${m}.tx must be a pin name or null")
    endif()

    _ryb_json_type(rx_type "${self}" "modules.${m}.rx" rx)
    if(NOT rx_type STREQUAL "ARRAY")
        message(FATAL_ERROR "ryb_comms.json: modules.${m}.rx must be a list, [] for none")
    endif()
    _ryb_json(rx "${self}" "modules.${m}.rx" rx)
    string(JSON rx_count LENGTH "${rx}")
    set(accept_mask 0)
    set(rx_links "")
    set(rx_bits "")
    set(rx_pins "")
    if(rx_count GREATER 0)
        math(EXPR last "${rx_count} - 1")
        foreach(i RANGE ${last})
            set(where "modules.${m}.rx[${i}]")
            _ryb_json(pin "${rx}" "${where}.pin" ${i} pin)
            _ryb_pin_bit(bit "${pins}" "${pin}" "${where}.pin")
            if(bit IN_LIST lcd_bits)
                message(FATAL_ERROR "ryb_comms.json: ${where}.pin ${pin} is also the LCD's "
                    "${lcd_role_of_${bit}} pin")
            endif()
            if(bit IN_LIST rx_bits)
                message(FATAL_ERROR
                    "ryb_comms.json: ${where}.pin ${pin} is already an rx link of ${m}")
            endif()
            if(bit EQUAL tx_bit AND NOT open_drain)
                # Two push-pull drivers on one lane is a short the moment they disagree.
                message(FATAL_ERROR "ryb_comms.json: ${where}.pin ${pin} is also the tx pin of "
                    "${m}; a shared lane needs line.drive \"open_drain\"")
            endif()
            list(APPEND rx_bits ${bit})
            list(APPEND rx_pins ${pin})

            _ryb_json_type(from_type "${rx}" "${where}.from" ${i} from)
            if(NOT from_type STREQUAL "ARRAY")
                message(FATAL_ERROR "ryb_comms.json: ${where}.from must be a list of module names")
            endif()
            _ryb_json(from "${rx}" "${where}.from" ${i} from)
            string(JSON from_count LENGTH "${from}")
            if(from_count EQUAL 0)
                message(FATAL_ERROR "ryb_comms.json: ${where}.from lists no senders")
            endif()
            set(link_mask 0)
            math(EXPR from_last "${from_count} - 1")
            foreach(j RANGE ${from_last})
                string(JSON sender GET "${from}" ${j})
                if(NOT DEFINED address_of_${sender})
                    message(FATAL_ERROR
                        "ryb_comms.json: ${where}.from names '${sender}', which is not a module")
                endif()
                if(repeater AND sender STREQUAL m)
                    message(FATAL_ERROR "ryb_comms.json: ${m} is a repeater and lists itself in "
                        "${where}.from, so it would forward its own packets forever")
                endif()
                math(EXPR link_mask "${link_mask} | (1 << ${address_of_${sender}})")
            endforeach()
            math(EXPR accept_mask "${accept_mask} | ${link_mask}")
            math(EXPR link_mask_hex "${link_mask}" OUTPUT_FORMAT HEXADECIMAL)
            list(APPEND rx_links "{ ${bit}u, ${link_mask_hex}u }")
        endforeach()
    endif()

    if(repeater AND (tx_bit EQUAL -1 OR rx_count EQUAL 0))
        message(FATAL_ERROR
            "ryb_comms.json: ${m} is a repeater, so it needs a tx pin and at least one rx link")
    endif()

    if(rx_count GREATER 0)
        list(JOIN rx_links ", " rx_links_c)
        set(rx_links_c "{ ${rx_links_c} }")
        list(JOIN rx_pins ", " rx_desc)
    else()
        # A placeholder entry keeps the C array non-empty; RYB_CFG_RX_LINK_COUNT stays 0.
        set(rx_links_c "{ { 0u, 0x0u } }")
        set(rx_desc "none")
    endif()

    # --- Receive callbacks: one per sender this module expects, named after both ------------
    set(callbacks "")
    set(callback_names "")
    foreach(addr RANGE 3)
        math(EXPR expected "(${accept_mask} >> ${addr}) & 1")
        if(NOT expected)
            continue()
        endif()
        set(sender ${name_of_${addr}})
        if(addr EQUAL command_address)
            set(fn "${prefix}_receive_command")
            string(APPEND callbacks
                "void ${fn}(uint8_t first, uint8_t second);\n"
                "#define RYB_CFG_DELIVER_${addr}(p) "
                "${fn}((p)->u.command.first, (p)->u.command.second)\n")
        else()
            set(fn "${prefix}_receive_${sender}")
            string(APPEND callbacks
                "void ${fn}(uint8_t ${_RYB_VALUE_${sender}}, ryb_trend_t trend);\n"
                "#define RYB_CFG_DELIVER_${addr}(p) "
                "${fn}((p)->u.appended.data, (p)->u.appended.trend)\n")
        endif()
        string(APPEND callbacks "#define RYB_CFG_CALLBACK_${addr} ${fn}\n")
        list(APPEND callback_names ${fn})
    endforeach()
    if(callback_names)
        list(JOIN callback_names ", " callback_desc)
    else()
        set(callbacks "/* None: ryb_comms.json gives this module no rx links. */\n")
        set(callback_desc "none")
    endif()

    # --- Generate the header and the library ---------------------------------------------------
    set(RYB_CFG_MODULE "${m}")
    set(RYB_CFG_JSON_NAME "${json_name}")
    set(RYB_CFG_ADDRESS ${address})
    set(RYB_CFG_COMMAND_ADDRESS ${command_address})
    if(address EQUAL command_address)
        set(RYB_CFG_IS_COMMAND_SENDER 1)
    else()
        set(RYB_CFG_IS_COMMAND_SENDER 0)
    endif()
    set(RYB_CFG_REPEATER ${repeater})
    set(RYB_CFG_GPIO_BASE "${base}")
    set(RYB_CFG_DIRECTION_SETTLE_US ${settle})
    set(RYB_CFG_BIT_PERIOD_US ${bit_period})
    set(RYB_CFG_IDLE_SLEEP_US ${idle_sleep})
    set(RYB_CFG_OPEN_DRAIN ${open_drain})
    set(RYB_CFG_THREAD_REALTIME ${realtime})
    set(RYB_CFG_THREAD_PRIORITY ${priority})
    set(RYB_CFG_THREAD_CPU ${cpu})
    set(RYB_CFG_TX_BIT ${tx_bit})
    set(RYB_CFG_RX_LINK_COUNT ${rx_count})
    set(RYB_CFG_RX_LINKS "${rx_links_c}")
    set(RYB_CFG_CALLBACKS "${callbacks}")
    math(EXPR RYB_CFG_ACCEPT_MASK "${accept_mask}" OUTPUT_FORMAT HEXADECIMAL)
    set(sender_names "")
    set(value_names "")
    foreach(addr RANGE 3)
        if(DEFINED name_of_${addr})
            set(sender ${name_of_${addr}})
            list(APPEND sender_names "\"${sender}\"")
            if(DEFINED _RYB_VALUE_${sender})
                list(APPEND value_names "\"${_RYB_VALUE_${sender}}\"")
            else()
                list(APPEND value_names "\"data\"")
            endif()
        else()
            list(APPEND sender_names "\"?\"")
            list(APPEND value_names "\"data\"")
        endif()
    endforeach()
    list(JOIN sender_names ", " sender_names)
    list(JOIN value_names ", " value_names)
    set(RYB_CFG_SENDER_NAMES "{ ${sender_names} }")
    set(RYB_CFG_TX_NAME "${tx_desc}")
    set(RYB_CFG_RX_NAMES "${rx_desc}")

    # The whole pins table, for tools that look at every pin (ryb_pinfind).
    string(JSON pin_count LENGTH "${pins}")
    set(pin_entries "")
    if(pin_count GREATER 0)
        math(EXPR last "${pin_count} - 1")
        foreach(i RANGE ${last})
            string(JSON pin_name MEMBER "${pins}" ${i})
            string(JSON pin_bit GET "${pins}" "${pin_name}")
            list(APPEND pin_entries "{ \"${pin_name}\", ${pin_bit}u }")
        endforeach()
    endif()
    list(JOIN pin_entries ", " pin_entries)
    set(RYB_CFG_PIN_COUNT ${pin_count})
    set(RYB_CFG_PINS "{ ${pin_entries} }")
    set(RYB_CFG_VALUE_NAMES "{ ${value_names} }")

    set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_config")
    configure_file("${RYB_COMMS_DIR}/ryb_comms_config.h.in" "${gen_dir}/ryb/ryb_comms_config.h"
        @ONLY)

    message(STATUS "RYB Comms ${target}: ${m} at address ${address}, tx ${tx_desc}, "
        "rx ${rx_desc}, receive callbacks to define: ${callback_desc}")

    if(arg_BACKEND STREQUAL "mmio")
        set(lcd_source ryb_lcd.c)
    else()
        set(lcd_source ryb_lcd_sim.c)
    endif()
    add_library(${target} STATIC
        "${RYB_COMMS_DIR}/ryb.c"
        "${RYB_COMMS_DIR}/ryb_comms.c"
        "${RYB_COMMS_DIR}/ryb_packet.c"
        "${RYB_COMMS_DIR}/ryb_gpio_${arg_BACKEND}.c"
        "${RYB_COMMS_DIR}/${lcd_source}")
    target_include_directories(${target} PUBLIC "${RYB_INCLUDE_DIR}" "${gen_dir}")
    target_compile_definitions(${target} PRIVATE _FILE_OFFSET_BITS=64)
    target_link_libraries(${target} PUBLIC Threads::Threads)
    _ryb_warnings(${target})
endfunction()

function(ryb_add_module name)
    set(module_dir "${RYB_REPO_ROOT}/modules/${name}")
    if(NOT EXISTS "${module_dir}/src/main.c")
        message(FATAL_ERROR "RYB_MODULE=${name}: ${module_dir}/src/main.c does not exist")
    endif()
    set(backend "${RYB_GPIO_BACKEND}")
    if(NOT backend)
        set(backend mmio)
    endif()
    ryb_comms_add_library(ryb_comms_${name} MODULE ${name} BACKEND ${backend})

    # The monitor runs the comms for this module with no module code: it shows what arrives and
    # sends test values. For bring-up and chain tests before main.c is written.
    add_executable(${name}_monitor "${RYB_COMMS_DIR}/ryb_monitor.c")
    target_link_libraries(${name}_monitor PRIVATE ryb_comms_${name})
    set_target_properties(${name}_monitor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    _ryb_warnings(${name}_monitor)

    # A dummy main.c for this module, showing how to use the library. Not built by default, so a
    # JSON change that alters the callbacks cannot break a deploy: build it with
    # cmake --build build --target <module>_example.
    if(EXISTS "${RYB_COMMS_DIR}/examples/${name}_main.c")
        add_executable(${name}_example EXCLUDE_FROM_ALL "${RYB_COMMS_DIR}/examples/${name}_main.c")
        target_link_libraries(${name}_example PRIVATE ryb_comms_${name})
        set_target_properties(${name}_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
        _ryb_warnings(${name}_example)
    endif()

    # The pin finder only needs the pins table and the GPIO, which every module's build shares.
    if(NOT TARGET ryb_pinfind)
        add_executable(ryb_pinfind "${RYB_COMMS_DIR}/ryb_pinfind.c")
        target_link_libraries(ryb_pinfind PRIVATE ryb_comms_${name})
        set_target_properties(ryb_pinfind PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
        _ryb_warnings(ryb_pinfind)
    endif()

    # A main.c that is still the template's placeholder would fail the whole build, so the module
    # program is only built by default once main() exists.
    file(READ "${module_dir}/src/main.c" main_source)
    if(main_source MATCHES "[^A-Za-z0-9_]main[ \t]*\\(")
        set(exclude "")
    else()
        set(exclude EXCLUDE_FROM_ALL)
        message(STATUS "RYB: modules/${name}/src/main.c has no main() yet, so `${name}` is not "
            "built by default; ${name}_monitor is")
    endif()

    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${module_dir}/src/*.c")
    add_executable(${name} ${exclude} ${sources})
    target_include_directories(${name} PRIVATE "${module_dir}/include" "${module_dir}/src")
    target_link_libraries(${name} PRIVATE ryb_comms_${name})
    if(UNIX)
        target_link_libraries(${name} PRIVATE m)
    endif()
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    _ryb_warnings(${name})
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Only one send function is declared per module, so calling the other must not compile.
        target_compile_options(${name} PRIVATE -Werror=implicit-function-declaration)
    endif()

    # Extra libraries a module needs (an LCD driver, ...) go here, not in this shared file.
    if(EXISTS "${module_dir}/module.cmake")
        set(RYB_MODULE_TARGET ${name})
        include("${module_dir}/module.cmake")
    endif()
endfunction()
