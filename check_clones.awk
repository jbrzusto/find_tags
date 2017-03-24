#!/usr/bin/gawk

/===/{
    split("",z,//);
    split("",x,//);
    split("",y,//);
    next;
}

/Examining/{
    if ($2 in z)
        print "Re-examining " $2;
    z[$2] = "";
    next;
}

/Delet/{
    delete z[$2];
    next;
}

/Cloned/{
    if ($2 in x)
        print "recloning " $2;
    x[$2]="";
    y[$4]="";
    if ($2 in y)
        print "cloning clone "  $2;
    if ($4 in z)
        print "got clone of prexisting " $4;
    next;
}
