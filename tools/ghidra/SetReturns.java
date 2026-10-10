// Give the listed functions a 32-bit return value in EAX (their callers use EAX after the call).
// Usage: -postScript SetReturns.java <file with hex addresses>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import ghidra.program.model.symbol.SourceType;
import java.io.*;
public class SetReturns extends GhidraScript {
    public void run() throws Exception {
        int n = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(getScriptArgs()[0]))) {
            String l;
            while ((l = r.readLine()) != null) {
                l = l.trim(); if (l.isEmpty()) continue;
                Function f = getFunctionAt(toAddr(l));
                if (f == null) { println("no function " + l); continue; }
                println("was " + l + " " + f.getReturnType().getName());
                f.setReturnType(Undefined4DataType.dataType, SourceType.USER_DEFINED);
                n++;
            }
        }
        println("return types set: " + n);
    }
}
