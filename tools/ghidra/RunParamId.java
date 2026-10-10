// Run Ghidra's "Decompiler Parameter ID" analyzer directly over all executable code, committing
// parameters and returns (including register-passed parameters) for every function with default signatures.
import ghidra.app.script.GhidraScript;
import ghidra.app.plugin.core.analysis.DecompilerFunctionAnalyzer;
import ghidra.app.util.importer.MessageLog;
import ghidra.framework.options.Options;
import ghidra.program.model.listing.Program;
public class RunParamId extends GhidraScript {
    public void run() throws Exception {
        DecompilerFunctionAnalyzer a = new DecompilerFunctionAnalyzer();
        Options opts = currentProgram.getOptions(Program.ANALYSIS_PROPERTIES);
        a.registerOptions(opts.getOptions("Decompiler Parameter ID"), currentProgram);
        a.optionsChanged(opts.getOptions("Decompiler Parameter ID"), currentProgram);
        MessageLog log = new MessageLog();
        boolean ok = a.added(currentProgram, currentProgram.getMemory().getExecuteSet(), monitor, log);
        println("parameter id analysis complete ok=" + ok + " log=" + log.toString().length());
    }
}
