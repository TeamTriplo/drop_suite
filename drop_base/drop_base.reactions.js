/**
 * @file
 * Shared click handler for emoji reaction bars rendered by
 * drop_base_render_reactions().
 *
 * Each bar carries a `data-react-url` attribute pointing at the consumer's
 * toggle endpoint. The expected JSON response shape is:
 *   { reactions: { code: { count, user_reacted, names } } }
 */
(function ($) {
  Backdrop.behaviors.dropBaseReactions = {
    attach: function (context, settings) {
      $('.drop-reactions', context).once('drop-reactions').each(function () {
        var $bar = $(this);
        var url  = $bar.attr('data-react-url');
        if (!url) {
          return;
        }

        $bar.on('click', '.react-btn', function () {
          var $btn  = $(this);
          var emoji = $btn.attr('data-emoji');
          if (!emoji) {
            return;
          }
          $btn.addClass('is-loading');

          $.ajax({
            url: url,
            type: 'POST',
            data: { emoji: emoji },
            dataType: 'json',
            success: function (data) {
              if (!data || !data.reactions) {
                return;
              }
              $bar.find('.react-btn').each(function () {
                var $b   = $(this);
                var code = $b.attr('data-emoji');
                if (!data.reactions[code]) {
                  return;
                }
                var info  = data.reactions[code];
                var char  = $b.attr('data-char') || '';
                var label = info.count > 0 ? char + ' ' + info.count : char;
                $b.text(label);
                $b.toggleClass('is-active', !!info.user_reacted);
                $b.toggleClass('react-btn--empty', info.count === 0);
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
