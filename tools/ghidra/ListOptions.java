import ghidra.app.script.GhidraScript;
import ghidra.framework.options.Options;
import ghidra.program.model.listing.Program;
public class ListOptions extends GhidraScript {
    public void run() throws Exception {
        Options opts = currentProgram.getOptions(Program.ANALYSIS_PROPERTIES);
        for (String n : opts.getOptionNames()) if (n.toLowerCase().contains("param") || n.toLowerCase().contains("decompiler")) println("OPT " + n + " = " + opts.getObject(n, null));
    }
}
