/**
 * Sample JavaScript class for testing.
 */
class Calculator {
    constructor(precision) {
        this.precision = precision || 2;
    }

    /**
     * Add two numbers.
     */
    add(a, b) {
        return parseFloat((a + b).toFixed(this.precision));
    }

    subtract(a, b) {
        return parseFloat((a - b).toFixed(this.precision));
    }
}

/**
 * Utility function for formatting.
 */
function formatNumber(num, decimals) {
    return num.toFixed(decimals);
}

const DEFAULT_PRECISION = 2;

module.exports = { Calculator, formatNumber, DEFAULT_PRECISION };
