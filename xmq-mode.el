;;; xmq-mode.el --- xmq syntax highlighting

;; Copyright © 2019, by Fredrik Öhrström

;; Author: Fredrik Öhrström (oehrstroem@gmail.com)
;; Version: 0.0.1
;; Keywords: languages

;; This file is not part of GNU Emacs.

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;; This package provides a major mode with highlighting for output
;; from `xmq'.

;;; Code:

;; create the list for font-lock.
;; each category of keyword is given a particular face
(set (make-local-variable 'font-lock-multiline) t)
(defvar xmq-font-lock-keywords)
(setq xmq-font-lock-keywords `(
                               ("\\('.*'\\)" . (1 font-lock-string-face))
                               ("\\(//.*\\)$" . (1 font-lock-comment-face))
                               ("^\\([^ /=()]*\\) *[({$]" . (1 font-lock-function-name-face))
                               (" \\([^ /=()]*\\) *[({$]" . (1 font-lock-function-name-face))
                               ("^\\([^ /=()]*\\) *=" . (1 font-lock-variable-name-face))
                               ("[ ({]\\([^ /=()]*\\) *=" . (1 font-lock-variable-name-face))
                               (" *= *\\([^ /={}()]*\\)" . (1 font-lock-string-face))
                               )
)

;;;###autoload
(define-derived-mode xmq-mode fundamental-mode
  "xmq"
  "Major mode for xmq output."
  (setq font-lock-defaults '((xmq-font-lock-keywords)))
)

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.xmq\\'" . xmq-mode))

;; add the mode to the `features' list
(provide 'xmq-mode)

(defun xmq-region (&optional b e)
  (interactive "r")
  (call-process-region b e "xmq" t t nil "-")
  (xmq-mode))

(defun buffer-contains-substring (string)
(save-excursion
  (save-match-data
    (goto-char (point-min))
    (search-forward string nil t))))

;; Very rudimentary check if the buffer contains xml or not.
;; Simply look for "<" "</" and ">". If all three are found,
;; then the buffer contains xml formatted code.
(defun xmq-buffer ()
  "Switch back and forth between xml and xmq."
  (interactive "")
  (let ((pos (point)))
    (if (and
         (buffer-contains-substring "<")
         (buffer-contains-substring "</")
         (buffer-contains-substring ">")) (xmq-mode) (xml-mode))
    (call-process-region (point-min) (point-max) "xmq" t t nil "-")
    (goto-char pos)
  ))

;; Local Variables:
;; coding: utf-8
;; End:

;;; xmq-mode.el ends here
