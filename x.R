ff0 <- read.table("0_ff.R", header=TRUE)
ff1 <- read.table("1_ff.R", header=TRUE)
ff2 <- read.table("2_ff.R", header=TRUE)
ff3 <- read.table("3_ff.R", header=TRUE)
gg0 <- read.table("0_11.R", header=TRUE)
gg1 <- read.table("1_11.R", header=TRUE)
gg2 <- read.table("2_11.R", header=TRUE)
gg3 <- read.table("3_11.R", header=TRUE)

ff0y <- ((ff0$msrdata1 - ff0$msrdata) / (ff0$mperf1 - ff0$mperf0))
ff1y <- ((ff1$msrdata1 - ff1$msrdata) / (ff1$mperf1 - ff1$mperf0))
ff2y <- ((ff2$msrdata1 - ff2$msrdata) / (ff2$mperf1 - ff2$mperf0))
ff3y <- ((ff3$msrdata1 - ff3$msrdata) / (ff3$mperf1 - ff3$mperf0))

gg0y <- ((gg0$msrdata1 - gg0$msrdata) / (gg0$mperf1 - gg0$mperf0))
gg1y <- ((gg1$msrdata1 - gg1$msrdata) / (gg1$mperf1 - gg1$mperf0))
gg2y <- ((gg2$msrdata1 - gg2$msrdata) / (gg2$mperf1 - gg2$mperf0))
gg3y <- ((gg3$msrdata1 - gg3$msrdata) / (gg3$mperf1 - gg3$mperf0))

