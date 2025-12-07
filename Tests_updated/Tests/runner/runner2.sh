#!/bin/bash
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
REDBG='\e[41m'
GREENBG='\e[42m'
BLUEBG='\e[44m'
NC='\033[0m' # No Color

TIMEOUT=40
TESTS_INPUT=`pwd`/tests/inputs
TESTS_GLOB=$TESTS_INPUT/${1:-test_*}
TESTS_OUTPUT=`pwd`/tests/outputs
TESTS_EXPECTED=`pwd`/tests/expected
CLEANER="python3 `pwd`/tests/runner/output_cleaner.py"
SMASH=`pwd`/smash
RUNNER=`pwd`/tests/runner/runner
TMP_FOLDER=/tmp/smash_test
KEEP_ORIG=${KEEP_ORIG:-0}
VALGRIND=${VALGRIND:-0}
VALGRIND_PATH=`which valgrind`
VALGRIND_OK_LINE="All heap blocks were freed -- no leaks are possible"

mkdir -p $TESTS_OUTPUT
rm -rf $TMP_FOLDER
cp -r ./tests/required_folder $TMP_FOLDER
cd $TMP_FOLDER

echo ""
echo "Running tests sequentially and checking results immediately..."
echo ""

i=0
overall_status=0  # Will track if any test fails
tests_list=($TESTS_GLOB)
total_tests=${#tests_list[@]}

for test_file in "${tests_list[@]}"; do
    test=$(basename -- "$test_file" .txt)
    ((i++))
    echo -e "[${i}/${total_tests}] Running test: ${YELLOW}${test}${NC}"

    # Run the test
    if [ $VALGRIND -eq 0 ] ; then
        $RUNNER $SMASH < $TESTS_INPUT/$test.txt > $TESTS_OUTPUT/$test.out 2>$TESTS_OUTPUT/$test.err
    else
        $RUNNER $VALGRIND_PATH --leak-check=full --show-reachable=yes --num-callers=20 \
        --track-fds=yes --log-file=$TESTS_OUTPUT/$test.valgrind --child-silent-after-fork=yes \
        $SMASH < $TESTS_INPUT/$test.txt > $TESTS_OUTPUT/$test.out 2>$TESTS_OUTPUT/$test.err
    fi

    # Clean output
    timeout $TIMEOUT $CLEANER $TESTS_OUTPUT/$test.out $TESTS_OUTPUT/$test.err
	fg_file="$TESTS_OUTPUT/test_fg.out.processed"
	if [[ -f "$fg_file" ]]; then
		sed -i -E 's/(& )[0-9]+/\12/' "$fg_file"
	fi
    # Initialize results as MISSING (in case expected files do not exist)
    output_result="${YELLOW}MISSING${NC}"
    err_result="${YELLOW}MISSING${NC}"
    valgrind_result=""
    test_failed=0 # Track if this test fails

    # Check stdout
    if [ -f "$TESTS_EXPECTED/$test.out.exp" ]; then
        if diff -q --strip-trailing-cr "$TESTS_EXPECTED/$test.out.exp" "$TESTS_OUTPUT/$test.out.processed" >/dev/null; then
            output_result="${GREEN}PASSED${NC}"
        else
            output_result="${RED}FAILED${NC}"
            test_failed=1
        fi
    fi

    # Check stderr
    if [ -f "$TESTS_EXPECTED/$test.err.exp" ]; then
        if diff -q --strip-trailing-cr "$TESTS_EXPECTED/$test.err.exp" "$TESTS_OUTPUT/$test.err.processed" >/dev/null; then
            err_result="${GREEN}PASSED${NC}"
        else
            err_result="${RED}FAILED${NC}"
            test_failed=1
        fi
    fi

    # Check valgrind if enabled
    if [ $VALGRIND -ne 0 ]; then
        if grep -q "$VALGRIND_OK_LINE" $TESTS_OUTPUT/$test.valgrind ; then
            valgrind_result=":${GREEN}PASSED${NC}"
        else
            valgrind_result=":${RED}FAILED${NC}"
            test_failed=1
        fi
    fi

    # Print results for this test
    if [ $VALGRIND -eq 0 ]; then
        echo -e "   STDOUT: $output_result  STDERR: $err_result"
    else
        # Remove leading colon if present in valgrind_result
        valgrind_display="${valgrind_result#:}"
        echo -e "   STDOUT: $output_result  STDERR: $err_result  VALGRIND: $valgrind_display"
    fi
    echo ""

    # If test failed, update overall status and do not remove files
    if [ $test_failed -eq 1 ]; then
        overall_status=1
    else
        # Test passed fully
        if [ $KEEP_ORIG -eq 0 ]; then
            rm -f $TESTS_OUTPUT/$test.out $TESTS_OUTPUT/$test.err
        fi
    fi
done

if [ $overall_status -eq 0 ]; then
    echo -e "${GREENBG}ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "${REDBG}SOME TESTS FAILED!${NC}"
    exit 1
fi
