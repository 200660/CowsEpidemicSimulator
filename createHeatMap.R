if (!require("gplots")) {
	install.packages("gplots", dependencies = TRUE)
	library(gplots)
}
input <- read.csv("heatmap.csv", check.names=FALSE)
heat_matrix <- data.matrix(input)
png(file = "heatmap.png", width= 8*300, height = 5.5*300, res=300, units = "px", bg="white")
heat_map <- heatmap.2(heat_matrix,
					main="Mapa cieplna zarajestrowanych pozycji krów przez RTLS",
					dendrogram="none",
					trace="none",
					key=FALSE,
					lhei=c(2.2,12),
					lwid=c(0.5,24),
					Rowv=FALSE,
					Colv=FALSE,
					col = terrain.colors(256),
					scale="column",
					offsetRow=0.5,
					margins=c(3.5,3.5))
dev.off()