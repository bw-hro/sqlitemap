#!/usr/bin/env bash

script_path=$(realpath "$0")
base_dir="$( dirname "$script_path" )/.."
build_dir="$base_dir/build"

${base_dir}/build.sh clean debug without_examples

echo "generate coverage.info:"
lcov --capture --directory . --output-file "${build_dir}/coverage.info" \
     --no-external --ignore-errors empty --ignore-errors mismatch \
     --rc geninfo_unexecuted_blocks=1

echo "generate filtered_coverage.info:"
lcov --extract "${build_dir}/coverage.info" '*/bw/sqlitemap/*' \
     --output-file "${build_dir}/filtered_coverage.info"

echo "generate coverage_summary.json"
lcov --summary "${build_dir}/filtered_coverage.info" > "${build_dir}/coverage_summary.txt"
total_line_coverage=$(grep -E "lines" "${build_dir}/coverage_summary.txt" | awk '{print $2}')
echo "{\"total_line_coverage\": \"$total_line_coverage\"}" > "${build_dir}/coverage_summary.json"

echo "generate coverage report:"

html_prolog="${build_dir}/coverage_report_prolog.html"
rm -f $html_prolog

echo '<html lang="en">' >> $html_prolog
echo '<head>' >> $html_prolog
echo '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">' >> $html_prolog
echo '<title>sqlitemap - @pagetitle@</title>' >> $html_prolog
echo '<link rel="stylesheet" type="text/css" href="@basedir@gcov.css">' >> $html_prolog
echo '</head>' >> $html_prolog
echo '<body>' >> $html_prolog
echo '<h1 style="font-family:sans-serif;text-align:center;">' >> $html_prolog
echo '<a href="../@basedir@">sqlitemap</a>: Simple key-value store based on Sqlite for C++' >> $html_prolog
echo '</h1><hr/>' >> $html_prolog

rm -Rf "${build_dir}/coverage_report"
genhtml "${build_dir}/filtered_coverage.info" -o "${build_dir}/coverage_report" --html-prolog "${build_dir}/coverage_report_prolog.html"

echo "download coverage badge"
curl -o "${build_dir}/coverage_report/badge.svg" "https://img.shields.io/badge/coverage-${total_line_coverage}25-rgb%2850,201,85%29"
