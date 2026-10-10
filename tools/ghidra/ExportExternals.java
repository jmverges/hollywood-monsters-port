import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
public class ExportExternals extends GhidraScript {
    public void run() throws Exception {
        try (PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[0], true))) {
            for (Function f : currentProgram.getFunctionManager().getExternalFunctions())
                w.println("EXTERNAL:" + f.getName() + "\t" + f.getName() + "\t0\tfalse\t\ttrue\t" + f.getCallingConventionName()
                    + "\t" + ExportRecomp.decl(f.getReturnType(), "@") + "\t" + f.getSignature().getPrototypeString().replace('\t', ' '));
        }
    }
}
