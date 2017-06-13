// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.application.validation;

import com.yahoo.test.ManualClock;
import com.yahoo.vespa.model.application.validation.xml.ValidationOverridesXMLReader;
import org.junit.Test;
import org.xml.sax.SAXException;

import java.io.IOException;
import java.io.StringReader;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.Optional;

import static org.junit.Assert.fail;
import static org.junit.Assert.assertEquals;

/**
 * @author bratseth
 */
public class ValidationOverrideTest {

    @Test
    public void testValidationOverridesInIsolation() throws IOException, SAXException {
        String validationOverrides =
                "<validation-overrides>" +
                "  <allow until='2000-01-01'>indexing-change</allow>" +
                "  <allow until='2000-01-03' comment='any text'>indexing-mode-change</allow>" +
                "</validation-overrides>";

        {

            ValidationOverrides overrides = new ValidationOverridesXMLReader().read(Optional.of(new StringReader(validationOverrides)),
                                                                                    ManualClock.at("2000-01-01T23:59:00"));
            assertOverridden("indexing-change", overrides);
            assertOverridden("indexing-mode-change", overrides);
            assertNotOverridden("field-type-change", overrides);
        }

        {
            ValidationOverrides overrides = new ValidationOverridesXMLReader().read(Optional.of(new StringReader(validationOverrides)),
                                                                                    ManualClock.at("2000-01-02T00:00:00"));
            assertNotOverridden("indexing-change", overrides);
            assertOverridden("indexing-mode-change", overrides);
            assertNotOverridden("field-type-change", overrides);
        }

        {
            ValidationOverrides overrides = new ValidationOverridesXMLReader().read(Optional.of(new StringReader(validationOverrides)),
                                                                                    ManualClock.at("2000-01-04T00:00:00"));
            assertNotOverridden("indexing-change", overrides);
            assertNotOverridden("indexing-mode-change", overrides);
            assertNotOverridden("field-type-change", overrides);
        }

    }

    @Test
    public void testInvalidOverridePeriod() throws IOException, SAXException {
        String validationOverrides =
                "<validation-overrides>" +
                "  <allow until='2000-02-02'>indexing-change</allow>" +
                "</validation-overrides>";

        try {
            new ValidationOverridesXMLReader().read(Optional.of(new StringReader(validationOverrides)),
                                                    ManualClock.at("2000-01-01T23:59:00"));
            fail("Expected validation interval override validation validation failure");
        }
        catch (IllegalArgumentException e) {
            assertEquals("validation-overrides is invalid", e.getMessage());
            assertEquals("allow 'indexing-change' until 2000-02-03T00:00:00Z is too far in the future: Max 30 days is allowed",
                         e.getCause().getMessage());
        }
    }

    private void assertOverridden(String validationId, ValidationOverrides overrides) {
        overrides.invalid(ValidationId.from(validationId).get(), "message"); // should not throw exception
    }

    private void assertNotOverridden(String validationId, ValidationOverrides overrides) {
        try {
            overrides.invalid(ValidationId.from(validationId).get(), "message");
            fail("Expected '" + validationId + "' to not be overridden");
        }
        catch (ValidationOverrides.ValidationException expected) {
        }
    }

}
