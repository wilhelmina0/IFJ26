#!/usr/bin/env bash
PASS=0
FAIL=0

check_case() {
    local file="$1"
    local exp_cc="$2"
    local exp_ic="$3"
    local exp_out="$4"

    ./ifj26.exe < "$file" > out.code 2> err.txt
    local cc=$?

    if [ "$cc" -ne "$exp_cc" ]; then
        echo "[FAIL] $file: compiler rc=$cc (expected $exp_cc)"
        cat err.txt
        FAIL=$((FAIL + 1))
        return
    fi

    if [ "$exp_cc" -eq 0 ]; then
        actual_out=$(./ic26int-windows-x86_64.exe out.code 2> ic_err.txt | tr -d '\r')
        local ic=$?
        if [ "$ic" -ne "$exp_ic" ]; then
            echo "[FAIL] $file: ic26int rc=$ic (expected $exp_ic)"
            cat ic_err.txt
            FAIL=$((FAIL + 1))
            return
        fi
        if [ -n "$exp_out" ] && [ "$actual_out" != "$exp_out" ]; then
            echo "[FAIL] $file: output '$actual_out' != expected '$exp_out'"
            FAIL=$((FAIL + 1))
            return
        fi
    fi

    echo "[PASS] $file"
    PASS=$((PASS + 1))
}

check_case "tests/01_arith_and_double.ifj26"       0  0  $'13\n3\n12.50\n2.50'
check_case "tests/02_recursion_fact.ifj26"          0  0  $'120'
check_case "tests/03_while_and_shadowing.ifj26"     0  0  $'0 10 20 100'
check_case "tests/04_strings_and_builtins.ifj26"    0  0  $'9 Ahoj 65 B 3'
check_case "tests/05_inferred_return_and_unit.ifj26" 0 0  $'hi\n42'
check_case "tests/06_runtime_substr_oob.ifj26"      0  40 ""
check_case "tests/07_err1_bad_escape.ifj26"         1  0  ""
check_case "tests/08_err1_tab_indent.ifj26"         1  0  ""
check_case "tests/09_err2_missing_import.ifj26"     2  0  ""
check_case "tests/10_err2_funexp_in_call.ifj26"     2  0  ""
check_case "tests/11_err3_undef_var.ifj26"          3  0  ""
check_case "tests/12_err3_no_main.ifj26"            3  0  ""
check_case "tests/13_err4_dup_var.ifj26"            4  0  ""
check_case "tests/14_err5_wrong_arg_type.ifj26"     5  0  ""
check_case "tests/15_err5_main_with_param.ifj26"    5  0  ""
check_case "tests/16_err6_type_mismatch.ifj26"      6  0  ""
check_case "tests/17_err6_rec_no_rettype.ifj26"     6  0  ""
check_case "tests/18_err7_assign_val.ifj26"         7  0  ""
check_case "tests/19_err10_bare_return.ifj26"       10 0  ""

rm -f out.code err.txt ic_err.txt
echo "----------------------------------------"
echo "Passed: $PASS | Failed: $FAIL"
