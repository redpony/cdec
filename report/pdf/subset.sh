ani() {
    if [ "$useani" ] ; then
        echo "$@"
    else
        if [ "$2" ] ;then
            echo "$2"
        else
            perl -e '$_=shift;s/.*-//;print' -- "$@"
        fi
    fi
}

graph=compare.1stpass.tune.pdf

pdftk "second session.pdf" cat 209-211 213 output conclusion-subset.pdf

for useani in '' 1; do
    if [ "$useani" ] ; then
        aname="-animate"
    else
        aname=
    fi
    pdftk "first session.pdf" cat 1 7 8 `ani 10-18` 19-25 26-37 \
        52-65 66-71  73-76 `ani 77-80` 85-88 \
        `ani 89-101` 102-104 `ani 107-110` 111-118 127-132 \
        141-144 146-148 \
        157-159 162 `ani 163-165` 166-168 169 171 `ani 173-178` 179 180-190 192-195 \
        output first-subset$aname.pdf

    pdftk "second session.pdf" cat 70 71 76 79 `ani 82-84` 85  \
        96 97 98 99 101 \
        output second-subset$aname.pdf

    pdftk C="CLSPclosing.pdf" G=$graph cat 10-11 14 `ani 16-33` 34 `ani 35-44` 45 46 47 48 49 52 `ani 53-58` 59-62 \
        68 69-74 \
        G \
        78  \
        80 `ani 81-82` 85 `ani 87-90` 91-102 \
        output parsing-subset$aname.pdf

    pdftk first-subset$aname.pdf second-subset$aname.pdf parsing-subset$aname.pdf conclusion-subset.pdf \
        cat output subset$aname.pdf
    pdftk ws10-graehl.pdf subset$aname.pdf cat output all$aname.pdf

done

pdftk 'first session.pdf' cat 1-149 output first.pdf
pdftk 'first.pdf' 'second session.pdf' ../ws10-graehl.pdf cat  output all-uncut.pdf

