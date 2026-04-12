(function () {
  'use strict';

  Backdrop.behaviors.dropfinanceCharts = {
    attach: function (context, settings) {
      // Only run once on full page load.
      if (context !== document) {
        return;
      }

      var data = settings.dropfinance;
      if (!data) {
        return;
      }

      /**
       * Donut chart color palette.
       */
      var donutColors = [
        '#1565c0', '#2e7d32', '#c62828', '#ef6c00', '#6a1b9a',
        '#00838f', '#ad1457', '#4e342e', '#37474f', '#558b2f',
        '#d84315', '#283593', '#00695c', '#7b1fa2', '#f9a825',
        '#0277bd', '#c2185b', '#33691e', '#e65100', '#4527a0'
      ];

      /**
       * Helper to render a donut chart.
       */
      function renderDonut(canvasId, chartData, title) {
        var ctx = document.getElementById(canvasId);
        if (!ctx || !chartData || !chartData.labels || !chartData.labels.length) {
          return;
        }
        var colors = donutColors.slice(0, chartData.labels.length);
        new Chart(ctx, {
          type: 'doughnut',
          data: {
            labels: chartData.labels,
            datasets: [{
              data: chartData.values,
              backgroundColor: colors,
              borderWidth: 1,
              borderColor: '#fff'
            }]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: title,
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    var total = context.dataset.data.reduce(function (a, b) { return a + b; }, 0);
                    var pct = total > 0 ? ((context.parsed / total) * 100).toFixed(1) : 0;
                    return context.label + ': $' + context.parsed.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    }) + ' (' + pct + '%)';
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Line chart: balance, income, and expenses over 6 months.
       */
      var lineCtx = document.getElementById('dropfinance-line-chart');
      if (lineCtx && data.labels && data.labels.length) {
        new Chart(lineCtx, {
          type: 'line',
          data: {
            labels: data.labels,
            datasets: [
              {
                label: 'Balance',
                data: data.balance,
                borderColor: '#1565c0',
                backgroundColor: 'rgba(21, 101, 192, 0.1)',
                fill: true,
                tension: 0.3,
                pointRadius: 4,
                borderWidth: 2
              },
              {
                label: 'Income',
                data: data.income,
                borderColor: '#2e7d32',
                backgroundColor: 'transparent',
                borderDash: [5, 5],
                tension: 0.3,
                pointRadius: 3,
                borderWidth: 2
              },
              {
                label: 'Expenses',
                data: data.expenses,
                borderColor: '#c62828',
                backgroundColor: 'transparent',
                borderDash: [5, 5],
                tension: 0.3,
                pointRadius: 3,
                borderWidth: 2
              }
            ]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: data.labels.length + '-Month Cash Flow Projection',
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    return context.dataset.label + ': $' + context.parsed.y.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    });
                  }
                }
              }
            },
            scales: {
              y: {
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Bar chart: income vs expenses per month.
       */
      var barCtx = document.getElementById('dropfinance-bar-chart');
      if (barCtx && data.labels && data.labels.length) {
        new Chart(barCtx, {
          type: 'bar',
          data: {
            labels: data.labels,
            datasets: [
              {
                label: 'Income',
                data: data.income,
                backgroundColor: 'rgba(46, 125, 50, 0.7)',
                borderColor: '#2e7d32',
                borderWidth: 1
              },
              {
                label: 'Expenses',
                data: data.expenses,
                backgroundColor: 'rgba(198, 40, 40, 0.7)',
                borderColor: '#c62828',
                borderWidth: 1
              },
              {
                label: 'Personal',
                data: data.personal,
                backgroundColor: 'rgba(239, 108, 0, 0.7)',
                borderColor: '#ef6c00',
                borderWidth: 1
              }
            ]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: data.labels.length + '-Month Income vs Spending',
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    return context.dataset.label + ': $' + context.parsed.y.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    });
                  }
                }
              }
            },
            scales: {
              y: {
                beginAtZero: true,
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Stacked bar chart: revenue projection (recurring + pipeline).
       */
      var revenueStackCtx = document.getElementById('dropfinance-revenue-stack');
      if (revenueStackCtx && data.recurring_income && data.recurring_income.length) {
        new Chart(revenueStackCtx, {
          type: 'bar',
          data: {
            labels: data.labels,
            datasets: [
              {
                label: 'Recurring Revenue',
                data: data.recurring_income,
                backgroundColor: 'rgba(46, 125, 50, 0.7)',
                borderColor: '#2e7d32',
                borderWidth: 1
              },
              {
                label: 'Project Revenue',
                data: data.pipeline_income,
                backgroundColor: 'rgba(21, 101, 192, 0.7)',
                borderColor: '#1565c0',
                borderWidth: 1
              }
            ]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: 'Revenue Projection',
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    return context.dataset.label + ': $' + context.parsed.y.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    });
                  },
                  afterLabel: function (context) {
                    // Show individual project details when hovering over pipeline bar.
                    if (context.datasetIndex === 1 && data.pipeline_details) {
                      var details = data.pipeline_details[context.dataIndex];
                      if (details && details.length) {
                        return details;
                      }
                    }
                    return '';
                  },
                  afterBody: function (items) {
                    var total = 0;
                    items.forEach(function (item) {
                      total += item.parsed.y;
                    });
                    return 'Total: $' + total.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    });
                  }
                }
              }
            },
            scales: {
              x: { stacked: true },
              y: {
                stacked: true,
                beginAtZero: true,
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Historical profitability chart.
       */
      var profitCtx = document.getElementById('dropfinance-profitability-chart');
      if (profitCtx && data.profitability) {
        new Chart(profitCtx, {
          type: 'bar',
          data: {
            labels: data.profitability.labels,
            datasets: [
              {
                label: 'Income',
                data: data.profitability.income,
                backgroundColor: 'rgba(46, 125, 50, 0.7)',
                borderColor: '#2e7d32',
                borderWidth: 1,
                order: 2
              },
              {
                label: 'Business Expenses',
                data: data.profitability.expenses,
                backgroundColor: 'rgba(198, 40, 40, 0.7)',
                borderColor: '#c62828',
                borderWidth: 1,
                order: 2
              },
              {
                label: 'Personal',
                data: data.profitability.personal,
                backgroundColor: 'rgba(239, 108, 0, 0.7)',
                borderColor: '#ef6c00',
                borderWidth: 1,
                order: 2
              },
              {
                label: 'Net',
                type: 'line',
                data: data.profitability.net,
                borderColor: '#1565c0',
                backgroundColor: 'transparent',
                borderWidth: 2,
                pointRadius: 2,
                tension: 0.3,
                order: 1
              }
            ]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: 'Monthly Profitability',
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    var val = context.parsed.y;
                    var sign = val < 0 ? '-$' : '$';
                    return context.dataset.label + ': ' + sign + Math.abs(val).toLocaleString(undefined, {
                      minimumFractionDigits: 0,
                      maximumFractionDigits: 0
                    });
                  }
                }
              }
            },
            scales: {
              y: {
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Donut charts: expense and income category breakdown.
       */
      if (data.expenseDonut) {
        renderDonut('dropfinance-expense-donut', data.expenseDonut, 'Where Money Goes (Business)');
      }
      if (data.incomeDonut) {
        renderDonut('dropfinance-income-donut', data.incomeDonut, 'Where Money Comes From');
      }

      /**
       * Horizontal bar chart: expenses by category.
       */
      var expBarCtx = document.getElementById('dropfinance-expense-bar');
      if (expBarCtx && data.expenseBar) {
        new Chart(expBarCtx, {
          type: 'bar',
          data: {
            labels: data.expenseBar.labels,
            datasets: [{
              data: data.expenseBar.values,
              backgroundColor: data.expenseBar.colors,
              borderWidth: 0
            }]
          },
          options: {
            indexAxis: 'y',
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: 'Expenses by Category (Last 12 Months)',
                font: { size: 16 }
              },
              legend: { display: false },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    return '$' + context.parsed.x.toLocaleString(undefined, {
                      minimumFractionDigits: 2,
                      maximumFractionDigits: 2
                    });
                  }
                }
              }
            },
            scales: {
              x: {
                beginAtZero: true,
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Client revenue report: yearly revenue bar chart.
       */
      var yearlyCtx = document.getElementById('dropfinance-yearly-revenue');
      if (yearlyCtx && data.yearlyRevenue) {
        new Chart(yearlyCtx, {
          type: 'bar',
          data: {
            labels: data.yearlyRevenue.labels,
            datasets: [{
              label: 'Revenue',
              data: data.yearlyRevenue.values,
              backgroundColor: 'rgba(21, 101, 192, 0.7)',
              borderColor: '#1565c0',
              borderWidth: 1
            }]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              title: {
                display: true,
                text: 'Annual Revenue',
                font: { size: 16 }
              },
              tooltip: {
                callbacks: {
                  label: function (context) {
                    return '$' + context.parsed.y.toLocaleString(undefined, {
                      minimumFractionDigits: 0,
                      maximumFractionDigits: 0
                    });
                  }
                }
              }
            },
            scales: {
              y: {
                beginAtZero: true,
                ticks: {
                  callback: function (value) {
                    return '$' + value.toLocaleString();
                  }
                }
              }
            }
          }
        });
      }

      /**
       * Client revenue report: top clients donut.
       */
      if (data.topClients) {
        renderDonut('dropfinance-top-clients', data.topClients, 'Top 10 Clients (All Time)');
      }
    }
  };

})();
