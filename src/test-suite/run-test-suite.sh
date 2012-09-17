#!/bin/bash

shadow_build_dir=.test-suite-shadow-build
project_dir=..
log_file=run.log

if [ "$1" == "-u" ]; then
  run_unit=yes
  run_e2e=no
  config="CONFIG+=unit_tests"
elif [ "$1" == "-e" ]; then
  run_unit=no
  run_e2e=yes
  config="CONFIG+=end_to_end_tests"
elif [ "$1" == "-a" ]; then
  run_unit=yes
  run_e2e=yes
  config="CONFIG+=unit_tests end_to_end_tests"
else
  echo "Syntax:"
  echo "   $0 -u   # Run unit tests"
  echo "   $0 -e   # Run end to end tests"
  echo "   $0 -a   # Run all tests"
  exit 1
fi

set -e

rm -f $log_file
mkdir -p $shadow_build_dir
cd $shadow_build_dir
qmake $project_dir "$config"
make

if [ "$run_unit" == "yes" ]; then
  ( cd csv-document-unit-test && ./csvDocumentUnitTest | tee -a ../$log_file )
  ( cd conversation-rw-unit-test && ./conversationRwTest | tee -a ../$log_file )
  ( cd default-sanitizer-unit-test && ./defaultSanitizerUnitTest | tee -a ../$log_file ) 
  ( cd aiml-engine-unit-test && ./aimlEngineUnitTest | tee -a ../$log_file )
  ( cd simple-aiml-engine-unit-test && ./simpleAimlEngineUnitTest | tee -a ../$log_file )
  ( cd secure-stats-file-unit-test && ./secureStatsFileUnitTest | tee -a ../$log_file )
  ( cd stats-manager-unit-test && ./statsManagerUnitTest | tee -a ../$log_file )
  ( cd cipher-unit-test && ./cipherUnitTest | tee -a ../$log_file )
fi

if [ "$run_e2e" == "yes" ]; then
  ( cd end-to-end-test && ./endToEndTest | tee -a ../$log_file )
fi

echo
echo "SUMMARY"
cat $log_file | grep "********* Start\|Totals" | sed -e 's/Start testing of//g'
rm -f $log_file
echo 

cd $project_dir


