events=$1 #'instructions:u,ref-cycles:u'
workset=$2 #'test'
report_name=$3 #'performance_counters.csv'
 
case ${workset} in 
'test')
    args=1000
    output='output.txt'
    err='error.txt'
    work_dir="../"
    ;;
*)
    echo "invalid workset"
    exit
    ;;
esac

cp a.out ${work_dir}
cur_dir=`pwd`

cd ${work_dir}
perf stat -e ${events} -x, -o ${report_name} ./a.out ${args} > ${output} 2> ${err}

rm ${output}
rm ${err}
rm a.out
cd ${cur_dir}
mv ${work_dir}/${report_name} ${report_name}