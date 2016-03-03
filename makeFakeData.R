#!/usr/bin/Rscript

# generate fake data

library(RSQLite)
con = dbConnect(SQLite(), "testtagdb.sqlite")
t = dbGetQuery(con, "select * from tags")

n = 5

offset = runif(nrow(t), 0, 20)

p = NULL

for (i in 1:nrow(t)) {
    r = c(0:5, 8, 12)
    times = offset[i] + r * t$period[i]
    a = times
    for (g in c("param1", "param2", "param3")) {
        a = a + t[[g]][i] / 1000.0
        times = c(times, a)
    }
    ff = data.frame(ts = round(times, 4))
    p = rbind(p, cbind(a="p1", ff, dfreq = round(3.1 + runif(length(times), -1, 1), 3), sig = round(-50 + runif(length(times), -5, 5), 1), noise = round(-70 + runif(length(times), -5, 5), 1)))
}
p = p[order(p$ts),]
write.table(p, "testdata.csv", row.names=FALSE, quote=FALSE, col.names=FALSE, sep=",")
