#! /home/monads/.local/bin/Rscript

# get script arguments
args <- commandArgs(trailingOnly = TRUE);

# treat each argument
for (arg in args) 
{
	rollback_num <- as.integer(arg);
	
	input_file <- paste("result.", as.character(rollback_num), sep = "");
	input_data <- read.table(file = input_file, header = TRUE, sep = "\t");

	treated_data <- aggregate(input_data$"percent", list(input_data$"depth"), mean);

	output_file <- paste("treated_result.", as.character(rollback_num), sep = "");	
	write.table(treated_data, file = output_file, sep = "\t", row.names = FALSE, col.names = FALSE);
}

#rollback_num <- 2000;
#input_file <- paste("result.", as.character(rollback_num), sep = "");

# extract interested columns
#input <- read.table(file = input_file, header = TRUE, sep = "\t");
#interested_input <- result[, c(1, 4)];
#interested_input <- data.frame(input$"depth", input$"percent");

# grouping columns by the first and calculating the mean value of the rest
#calculated_result <- aggregate(input$"percent", list(input$"depth"), mean);

# output_file <- paste("treated_result.", as.character(rollback_num), sep = "");
# write.table(calculated_result, file = output_file, sep = "\t", row.names = FALSE, col.names = FALSE);


