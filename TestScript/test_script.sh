#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 server_name port" >&2
  exit 1
fi
./$1 $2 ./test &>test_log.txt &
echo "Starting server, saving log: $temp_log_file"

sleep 1s
server_pid=$!
if ps -p $server_pid > /dev/null
then
    echo "Server started with PID: $server_pid"
else
    echo "Error starting server, check test_log.txt"
    exit 1
fi
base_url="http://127.0.0.1:$2/" 
sub_url="${base_url}directory/"

web_root="./test/"
sub_root=${web_root}"directory/"
index_file="index.html"
css_file="style.css"
javascript_file="script.js"
jpeg_file="sample.jpg"
mime_html="Content-Type: text/html$"
mime_jpeg="Content-Type: image/jpeg$"
mime_css="Content-Type: text/css$"
mime_javascript="Content-Type: text/javascript$|Content-Type: application/javascript$"

do_http_get () {
    test_num=$1
    test_desc=$2
    test_url=$3
    test_file=$4
    test_resp=$5
    test_mime=$6

    temp_file="$(mktemp /tmp/myscript.XXXXXX)"
    
    temp_header="$(mktemp /tmp/myscript.XXXXXX)"
    
    header_pass=false
    get_pass=false
    mime_pass=false
    wget -q --server-response -O $temp_file $test_url 2> $temp_header
    if [ "$test_resp" == "404" ];then
        if grep -Eiq 'HTTP/1.0 404|HTTP/1.1 404' $temp_header 
        then
            header_pass=true
        fi
        get_pass=true
        mime_pass=true
    else
        if grep -Eiq 'HTTP/1.0 200 OK$|HTTP/1.1 200 OK$' $temp_header 
        then
            header_pass=true
        fi
        if grep -Eiq "$test_mime" $temp_header 
        then
            mime_pass=true
        fi
        diff $test_file $temp_file &>/dev/null
        diff_res=$?
        if [ $diff_res -eq 0 ];
        then
            get_pass=true
        else
            get_pass=false
        fi
    fi
    rm -f "$temp_file"
    rm -f "$temp_header"
    
    if $header_pass && $get_pass && $mime_pass;
    then
        echo "Test $test_num: $test_desc: PASS"
    else
        echo "Test $test_num: $test_desc: FAIL: header:$header_pass, get:$get_pass, mime:$mime_pass"
    fi
} 

do_http_get 1 "GET HTML file in root" $base_url$index_file $web_root$index_file "200" "$mime_html"
do_http_get 2 "GET Non-existent HTML file in root" $base_url"junk.html" $web_root$index_file "404"
do_http_get 3 "GET CSS file in root" $base_url$css_file $web_root$css_file "200" "$mime_css"
do_http_get 4 "GET JavaScript file in root" $base_url$javascript_file $web_root$javascript_file "200" "$mime_javascript"
do_http_get 5 "GET JPEG file in root" $base_url$jpeg_file $web_root$jpeg_file "200" "$mime_jpeg"
do_http_get 6 "GET HTML file in directory" "$sub_url$index_file" "$sub_root$index_file" "200" "$mime_html"
do_http_get 7 "GET CSS file in directory" "$sub_url$css_file" "$sub_root$css_file" "200" "$mime_css"
do_http_get 8 "GET JavaScript file in directory" "$sub_url$javascript_file" "$sub_root$javascript_file" "200" "$mime_javascript"
do_http_get 9 "GET JPEG file in directory" "$sub_url$jpeg_file" "$sub_root$jpeg_file" "200" "$mime_jpeg"


kill $server_pid
