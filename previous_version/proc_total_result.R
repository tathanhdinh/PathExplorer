#! /home/monads/.local/bin/Rscript

# read total result
total_result <- read.table(file = "total_result", header = TRUE, sep = "\t");

# calculate the percentage
resolved_ratio_table <- data.frame("max_rollback"  = total_result$"max_rollback", 
																	 "used_rollback" = total_result$"used_rollback", 
                                   "trace_depth"   = total_result$"trace_depth",
                                   "solved_ratio"  = total_result$"solved_branch" / total_result$"total_branch"); 
# aggregate data
aggregated_ratio_table <- aggregate(data.frame(solved_ratio = resolved_ratio_table$"solved_ratio", 
                                               used_rollback = resolved_ratio_table$"used_rollback"),
                                    by = list(resolved_ratio_table$"max_rollback", resolved_ratio_table$"trace_depth"),
                                    FUN = mean);
colnames(aggregated_ratio_table)[1:2] <- c("max_rollback", "trace_depth");
# aggregated_ratio_table;

dist_max_rollback_table <- unique(aggregated_ratio_table$"max_rollback");
for (dist_max_rollback in dist_max_rollback_table)
{
  output_table <- subset(aggregated_ratio_table, max_rollback == dist_max_rollback);
  # output_table <- aggregated_ratio_table[aggregated_ratio_table$"max_rollback" == dist_max_rollback, ];
  # output_table;

  output_file_name <- paste("treated_result_3d.", as.character(dist_max_rollback), sep = "");
  write.table(output_table[,c("trace_depth", "used_rollback", "solved_ratio")], 
              file = output_file_name, sep = "\t", row.names = FALSE, col.names = FALSE);

  output_file_name <- paste("treated_result_2d.", as.character(dist_max_rollback), sep = "");
  write.table(output_table[,c("trace_depth", "solved_ratio")], 
              file = output_file_name, sep = "\t", row.names = FALSE, col.names = FALSE);

}

# dist_max_rollback_table;

# colnames(aggregated_ratio_table) <- c("max_rollback", "trace_depth", "solved_ratio");
# aggregated_used_rollback_table <- aggregate(resolved_ratio_table$"used_rollback", 
#                                             by = list(resolved_ratio_table$"max_rollback", resolved_ratio_table$"trace_depth"), 
#                                             FUN = mean);

# colnames(aggregated_ratio_table) <- c("max_rollback", "trace_depth", "solved_ratio");
#aggregated_ratio_table$"used_rollback" = resolved_ratio_table$"used_rollback";

#aggregated_ratio_table;
#, total_result$"solved_branch" * total_result$"total_branch");