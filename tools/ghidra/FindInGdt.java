import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import java.io.File;
import java.util.*;
public class FindInGdt extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        FileDataTypeManager f = FileDataTypeManager.openFileArchive(new File(a[0]), false);
        for (int i = 1; i < a.length; i++) {
            List<DataType> found = new ArrayList<>();
            f.findDataTypes(a[i], found);
            for (DataType d : found) println("FOUND " + a[i] + " -> " + d.getPathName() + " len=" + d.getLength());
            if (found.isEmpty()) println("MISSING " + a[i]);
        }
        f.close();
    }
}
