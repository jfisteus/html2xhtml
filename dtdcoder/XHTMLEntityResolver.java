import java.io.File;
import java.io.IOException;

import java.net.URLConnection;
import java.net.URL;

import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;

/**
 * Workaround for W3C blocking requests with the User-Agent
 * header the XML parser sets.
 *
 */
public class XHTMLEntityResolver implements EntityResolver {
    String baseURL;
    String sep;

    public XHTMLEntityResolver(String dtdURL) {
        baseURL = dtdURL.replaceAll("/[^/]*\\z", "/");
        if (File.separatorChar == '\\') {
            sep = "\\\\";
        } else {
            sep = File.separator;
        }
    }

    public InputSource resolveEntity (String publicId, String systemId) {
        InputSource source = null;
        String resolvedUrl;
        if (!systemId.startsWith("http://")) {
            String end = systemId.replaceAll("\\A.*" + sep, "");
            if (publicId.startsWith("-//W3C//ENTITIES XHTML 1.1")
                || publicId.startsWith("-//W3C//ENTITIES XHTML-Print 1.0")) {
                resolvedUrl = baseURL + end;
            } else if (publicId.startsWith("-//W3C//ENTITIES")
                       || publicId.startsWith("-//W3C//ELEMENTS")) {
                resolvedUrl = "http://www.w3.org/TR/xhtml-modularization/DTD/"
                    + end;
            } else {
                resolvedUrl = baseURL + end;
            }
        } else {
            resolvedUrl = systemId;
        }
        System.err.println("Downloading " + resolvedUrl + "; " + publicId);
        try {
            URL url = new URL(resolvedUrl);
            URLConnection con = url.openConnection();
            con.setRequestProperty("User-Agent", "html2xhtml/DTDCoder");
            source = new InputSource(con.getInputStream());
        } catch (IOException ioe) {
            ioe.printStackTrace();
        }
        return source;
    }
}
