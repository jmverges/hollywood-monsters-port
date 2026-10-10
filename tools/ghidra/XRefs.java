// Print references to the given addresses with the containing function.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
public class XRefs extends GhidraScript {
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            for (Reference r : getReferencesTo(toAddr(a))) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("XREF " + a + " <- " + r.getFromAddress() + " " + (f == null ? "?" : f.getName()) + " " + r.getReferenceType());
            }
        }
    }
}
