/**
 * @file
 * DropTickets — datepicker, body toggle, emoji reactions, inline status, activity log.
 */

(function ($) {
  Backdrop.behaviors.dropticketsTypeProjectToggle = {
    attach: function (context, settings) {
      var $type = $('.droptickets-type-select', context).once('type-project-toggle');
      if (!$type.length) { return; }
      var $projectItem = $type.closest('.droptickets-form-sidebar').find('.form-item-pid');

      function toggleProject() {
        if ($type.val() === 'discussion') {
          $projectItem.hide();
        } else {
          $projectItem.show();
        }
      }

      toggleProject();
      $type.on('change', toggleProject);
    }
  };
}(jQuery));
(function ($) {
  Backdrop.behaviors.dropticketsDatpicker = {
    attach: function (context, settings) {
      if (!$.fn.datepicker) { return; }
      $('.droptickets-datepicker', context).once('droptickets-datepicker').datepicker({
        dateFormat: 'yy-mm-dd',
        changeMonth: true,
        changeYear: true
      });
    }
  };
}(jQuery));

(function ($) {
  Backdrop.behaviors.dropticketBodyToggle = {
    attach: function (context, settings) {
      $('.droptickets-body--collapsible', context).once('body-toggle').each(function () {
        var $card  = $(this);
        var $inner = $card.find('.droptickets-body-inner');
        var $btn   = $card.find('.droptickets-body-toggle');

        // If content is short enough, hide the toggle and fade entirely.
        if ($inner[0].scrollHeight <= 400) {
          $btn.hide();
          $card.find('.droptickets-body-fade').hide();
          $inner.css('max-height', 'none');
          return;
        }

        $btn.on('click', function () {
          var expanded = $card.hasClass('is-expanded');
          $card.toggleClass('is-expanded', !expanded);
          $btn.attr('aria-expanded', String(!expanded));
          $btn.text(!expanded ? Backdrop.t('Collapse') : Backdrop.t('Expand'));
        });
      });
    }
  };
}(jQuery));

(function ($) {
  Backdrop.behaviors.dropticketReactions = {
    attach: function (context, settings) {
      $('.droptickets-reactions', context).once('reactions').each(function () {
        var $bar = $(this);
        var tid  = $bar.data('tid');
        var tcid = $bar.data('tcid');
        var entityType = (tcid > 0) ? 'comment' : 'ticket';
        var entityId   = (tcid > 0) ? tcid : tid;

        $bar.find('.react-btn').on('click', function () {
          var $btn  = $(this);
          var emoji = $btn.data('emoji');

          $btn.addClass('is-loading');

          $.ajax({
            url: Backdrop.settings.basePath + 'drop/tickets/react/' + entityType + '/' + entityId,
            type: 'POST',
            data: { emoji: emoji },
            dataType: 'json',
            success: function (data) {
              if (!data.reactions) { return; }
              var emojis = (Backdrop.settings.droptickets || {}).reactionEmojis || {};
              $bar.find('.react-btn').each(function () {
                var $b   = $(this);
                var code = $b.data('emoji');
                if (!data.reactions[code]) { return; }
                var info  = data.reactions[code];
                var char  = emojis[code] || code;
                var label = info.count > 0 ? char + ' ' + info.count : char;
                $b.text(label);
                $b.toggleClass('is-active', info.user_reacted);
                // Update tooltip with reactor names.
                var title = info.names && info.names.length
                  ? char + ' ' + info.names.join(' · ')
                  : char;
                $b.attr('title', title);
              });
            },
            complete: function () {
              $btn.removeClass('is-loading');
            }
          });
        });
      });
    }
  };
}(jQuery));

(function ($) {
  /**
   * Inline status select on the ticket view page.
   *
   * On change, POSTs the new status to the inline-status endpoint.
   * Updates the select's color class on success; reverts on failure.
   */
  Backdrop.behaviors.dropticketsInlineStatus = {
    attach: function (context, settings) {
      $('.droptickets-status-select', context).once('droptickets-inline-status').on('change', function () {
        var $select   = $(this);
        var newStatus = $select.val();
        var oldStatus = $select.data('current-status') || $select.find('option[selected]').val();
        var endpoint  = $select.data('endpoint');
        var token     = $select.data('token');

        // Track the previous value for revert.
        if (!$select.data('current-status')) {
          $select.data('current-status', $select.find('option:selected').val());
        }
        var prevStatus = $select.data('current-status');

        $select.prop('disabled', true).addClass('droptickets-status-saving');

        $.ajax({
          url:  endpoint,
          type: 'POST',
          data: { status: newStatus, token: token },
          dataType: 'json',
          success: function (resp) {
            if (resp.ok) {
              // Update color class to match new status.
              $select.removeClass(function (i, cls) {
                return (cls.match(/droptickets-status--\S+/) || []).join(' ');
              });
              $select.addClass('droptickets-status--' + newStatus);
              $select.data('current-status', newStatus);
            }
            else {
              // Revert to previous value.
              $select.val(prevStatus);
              if (resp.message) {
                alert(resp.message);
              }
            }
          },
          error: function () {
            $select.val(prevStatus);
          },
          complete: function () {
            $select.prop('disabled', false).removeClass('droptickets-status-saving');
          }
        });
      });
    }
  };

  /**
   * Inline assignee select on the ticket view page.
   *
   * On change, POSTs the new assignee value to the inline-assignee endpoint.
   * Reverts on failure.
   */
  Backdrop.behaviors.dropticketsInlineAssignee = {
    attach: function (context, settings) {
      $('.droptickets-assignee-select', context).once('droptickets-inline-assignee').on('change', function () {
        var $select  = $(this);
        var newVal   = $select.val();
        var prevVal  = $select.data('current');
        var endpoint = $select.data('endpoint');
        var token    = $select.data('token');

        $select.prop('disabled', true).addClass('droptickets-saving');

        $.ajax({
          url:      endpoint,
          type:     'POST',
          data:     { assignee: newVal, token: token },
          dataType: 'json',
          success: function (resp) {
            if (resp.ok) {
              $select.data('current', newVal);
            }
            else {
              $select.val(prevVal);
              if (resp.message) { alert(resp.message); }
            }
          },
          error: function () {
            $select.val(prevVal);
          },
          complete: function () {
            $select.prop('disabled', false).removeClass('droptickets-saving');
          }
        });
      });
    }
  };

  /**
   * Review / triage toggle buttons.
   *
   * POSTs to the toggle endpoint and updates the button + row class in place
   * so the user can work through the list without page reloads.
   */
  Backdrop.behaviors.dropticketReview = {
    attach: function (context, settings) {
      $('.droptickets-review-btn', context).once('droptickets-review').on('click', function () {
        var $btn = $(this);
        var $row = $btn.closest('tr');
        var url  = $btn.data('url');

        $btn.prop('disabled', true);

        $.post(url, function (data) {
          if (data.reviewed) {
            $btn.addClass('droptickets-review-btn--done')
                .attr('title', Backdrop.t('Reviewed \u2014 click to unmark'));
            $row.addClass('droptickets-review-row--done');
          }
          else {
            $btn.removeClass('droptickets-review-btn--done')
                .attr('title', Backdrop.t('Mark as reviewed'));
            $row.removeClass('droptickets-review-row--done');
          }
        }, 'json').always(function () {
          $btn.prop('disabled', false);
        });
      });
    }
  };

}(jQuery));

(function ($) {
  /**
   * Toggle activity events in the ticket timeline.
   */
  Backdrop.behaviors.dropticketsActivityToggle = {
    attach: function (context, settings) {
      $('.droptickets-toggle-events', context).once('activity-toggle').on('click', function () {
        var $btn      = $(this);
        var $timeline = $btn.closest('.droptickets-view-main');
        var hidden    = $timeline.hasClass('droptickets-events-hidden');
        if (hidden) {
          $timeline.removeClass('droptickets-events-hidden');
          $btn.text($btn.data('label-hide'));
        }
        else {
          $timeline.addClass('droptickets-events-hidden');
          $btn.text($btn.data('label-show'));
        }
      });
    }
  };
}(jQuery));

(function ($) {
  /**
   * Image lightbox for ticket body and comment images.
   *
   * Clicking any image inside .droptickets-body-inner or
   * .droptickets-comment-body opens it in a fullscreen overlay.
   * Zoom in/out via +/- buttons, mouse wheel, or +/- keyboard keys.
   * Close by clicking the overlay background, the × button, or pressing Escape.
   */
  Backdrop.behaviors.dropticketImageLightbox = {
    attach: function (context, settings) {
      // Build the lightbox DOM once, on first attach.
      if (!$('#droptickets-lightbox').length) {
        var $lb = $(
          '<div id="droptickets-lightbox" class="droptickets-lightbox" role="dialog" aria-modal="true">' +
            '<div class="droptickets-lightbox-toolbar">' +
              '<button class="droptickets-lightbox-zoom-out" aria-label="' + Backdrop.t('Zoom out') + '">&#8722;</button>' +
              '<span class="droptickets-lightbox-zoom-level">100%</span>' +
              '<button class="droptickets-lightbox-zoom-in"  aria-label="' + Backdrop.t('Zoom in') + '">&#43;</button>' +
              '<button class="droptickets-lightbox-zoom-reset" aria-label="' + Backdrop.t('Reset zoom') + '">' + Backdrop.t('Reset') + '</button>' +
              '<button class="droptickets-lightbox-close" aria-label="' + Backdrop.t('Close') + '">&times;</button>' +
            '</div>' +
            '<div class="droptickets-lightbox-stage">' +
              '<img src="" alt="" />' +
            '</div>' +
          '</div>'
        );
        $('body').append($lb);

        var scale     = 1;
        var STEP      = 0.25;
        var MIN_SCALE = 0.25;
        var MAX_SCALE = 4;

        function applyZoom() {
          $lb.find('.droptickets-lightbox-stage img').css('transform', 'scale(' + scale + ')');
          $lb.find('.droptickets-lightbox-zoom-level').text(Math.round(scale * 100) + '%');
        }

        function zoomIn() {
          scale = Math.min(MAX_SCALE, +(scale + STEP).toFixed(2));
          applyZoom();
        }

        function zoomOut() {
          scale = Math.max(MIN_SCALE, +(scale - STEP).toFixed(2));
          applyZoom();
        }

        function resetZoom() {
          scale = 1;
          applyZoom();
        }

        function openLightbox(src, alt) {
          resetZoom();
          $lb.find('.droptickets-lightbox-stage img').attr({ src: src, alt: alt });
          $lb.addClass('is-open');
        }

        function closeLightbox() {
          $lb.removeClass('is-open');
        }

        // Zoom buttons.
        $lb.find('.droptickets-lightbox-zoom-in').on('click', function (e) {
          e.stopPropagation();
          zoomIn();
        });
        $lb.find('.droptickets-lightbox-zoom-out').on('click', function (e) {
          e.stopPropagation();
          zoomOut();
        });
        $lb.find('.droptickets-lightbox-zoom-reset').on('click', function (e) {
          e.stopPropagation();
          resetZoom();
        });

        // Close button.
        $lb.find('.droptickets-lightbox-close').on('click', function (e) {
          e.stopPropagation();
          closeLightbox();
        });

        // Close on overlay background click (not toolbar, not image).
        $lb.on('click', function (e) {
          if ($(e.target).is('#droptickets-lightbox, .droptickets-lightbox-stage')) {
            closeLightbox();
          }
        });

        // Mouse wheel zoom.
        $lb[0].addEventListener('wheel', function (e) {
          if (!$lb.hasClass('is-open')) { return; }
          e.preventDefault();
          if (e.deltaY < 0) { zoomIn(); } else { zoomOut(); }
        }, { passive: false });

        // Keyboard shortcuts.
        $(document).on('keydown.dropticket-lightbox', function (e) {
          if (!$lb.hasClass('is-open')) { return; }
          if (e.key === 'Escape')               { closeLightbox(); }
          else if (e.key === '+' || e.key === '=') { zoomIn(); }
          else if (e.key === '-')               { zoomOut(); }
          else if (e.key === '0')               { resetZoom(); }
        });

        // Expose openLightbox so the click handler below can use it.
        $lb.data('open', openLightbox);
      }

      // Attach click handler to images in body and comment areas.
      $('.droptickets-body-inner img, .droptickets-comment-body img', context)
        .once('dropticket-lightbox')
        .on('click', function () {
          var $img = $(this);
          $('#droptickets-lightbox').data('open')($img.attr('src'), $img.attr('alt') || '');
        });
    }
  };
}(jQuery));
