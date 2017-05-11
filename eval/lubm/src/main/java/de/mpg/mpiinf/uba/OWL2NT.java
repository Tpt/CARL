package de.mpg.mpiinf.uba;

import org.apache.jena.rdf.model.InfModel;
import org.apache.jena.rdf.model.Model;
import org.apache.jena.rdf.model.ModelFactory;
import org.apache.jena.reasoner.Reasoner;
import org.apache.jena.reasoner.ReasonerRegistry;
import org.apache.jena.reasoner.ValidityReport;
import org.apache.jena.util.FileManager;
import org.apache.jena.vocabulary.RDF;

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * @author Thomas Pellissier Tanon
 */
public class OWL2NT {

    public static void main(String[] args) throws IOException {
        if(args.length < 2) {
            System.out.println("You should specify the working directory and the output file");
            return;
        }
        boolean expand = true;
        if(args.length == 3 && args[2].equals("-no-expand")) {
            System.out.println("No reasoning");
            expand = false;
        }

        Model data = ModelFactory.createDefaultModel();
        Files.newDirectoryStream(Paths.get(args[0]), "*.owl").forEach(path -> {
            System.out.println("loading file: " + path.toString());
            data.add(FileManager.get().loadModel("file:" + path.toString()));
        });

        Model outputModel;
        if(expand) {
            Model schema = FileManager.get().loadModel("http://swat.cse.lehigh.edu/onto/univ-bench.owl");
            Reasoner reasoner = ReasonerRegistry.getOWLReasoner().bindSchema(schema);
            InfModel infmodel = ModelFactory.createInfModel(reasoner, data);

            ValidityReport reports = infmodel.validate();
            reports.getReports().forEachRemaining(report -> {
                System.out.println(report.toString());
            });
            if(!reports.isValid()) {
                System.out.println("Not valid file");
                return;
            } else {
                System.out.println("Valid file");
            }
            outputModel = infmodel;
        } else {
            outputModel = data;
        }

        try(PrintWriter writer = new PrintWriter(args[1])) {
            outputModel.listStatements().forEachRemaining(statement -> {
                if(statement.getObject().isURIResource() && !statement.getPredicate().equals(RDF.type)) {
                    writer.print(statement.getSubject());
                    writer.print('\t');
                    writer.print(statement.getPredicate());
                    writer.print('\t');
                    writer.print(statement.getObject());
                    writer.print('\n');
                }
            });
        }
    }
}
