// Run Ghidra's C/C++ exporter (header with types + globals, and all functions).
// Usage: -postScript ExportCpp.java <outfile.c>
import ghidra.app.script.GhidraScript;
import ghidra.app.util.exporter.CppExporter;
import ghidra.app.util.Option;
import java.io.File;
import java.util.*;

public class ExportCpp extends GhidraScript {
    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        CppExporter ex = new CppExporter();
        List<Option> opts = new ArrayList<>();
        opts.add(new Option(CppExporter.CREATE_HEADER_FILE, Boolean.TRUE));
        opts.add(new Option(CppExporter.CREATE_C_FILE, Boolean.TRUE));
        opts.add(new Option(CppExporter.USE_CPP_STYLE_COMMENTS, Boolean.TRUE));
        opts.add(new Option(CppExporter.EMIT_TYPE_DEFINITONS, Boolean.TRUE));
        opts.add(new Option(CppExporter.EMIT_REFERENCED_GLOBALS, Boolean.TRUE));
        ex.setOptions(opts);
        ex.export(out, currentProgram, null, monitor);
        println("exported " + out);
    }
}
