import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.app.services.DataTypeManagerService;
import java.util.*;
public class FindTypes extends GhidraScript {
    public void run() throws Exception {
        String[] names = getScriptArgs();
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        List<DataTypeManager> all = new ArrayList<>(); all.add(dtm);
        for (String n : names) {
            List<DataType> found = new ArrayList<>();
            dtm.findDataTypes(n, found);
            println("PROGRAM " + n + ": " + found);
        }
        // look at archives linked to the program
        for (SourceArchive sa : dtm.getSourceArchives()) println("ARCHIVE " + sa.getName());
    }
}
