;;; elfuse.el --- Emacs FUSE -*- lexical-binding: t -*-

;; This file is part of Elfuse.

;; Elfuse is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; Elfuse is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Elfuse.  If not, see <http://www.gnu.org/licenses/>.

(require 'elfuse-module)
(require 'seq)


(defconst elfuse-EPERM 1 "errno: operation not permitted")
(defconst elfuse-ENOENT 2 "errno: no such file or directory")
(defconst elfuse-EACCESS 13 "errno: permission denied")
(defconst elfuse-EBUSY 16 "errno: block device required")
(defconst elfuse-EEXIST 17 "errno: file exists")
(defconst elfuse-ENOTDIR 20 "errno: not a directory")
(defconst elfuse-EISDIR 21 "errno: is a directory")
(defconst elfuse-EINVAL 22 "errno: invalid argument")
(defconst elfuse-EROFS 30 "errno: read-only file system")
(defconst elfuse-ENOSYS 38 "errno: function not implemented")
(defconst elfuse-ENOTEMPTY 39 "errno: directory not empty")

(defvar elfuse-time-between-checks 0.01
  "Time interval in seconds between Elfuse request checks.")

(defconst elfuse--supported-ops-alist '((create . 1)
                                        (rename . 2)
                                        (readdir . 1)
                                        (getattr . 1)
                                        (open . 1)
                                        (release . 1)
                                        (read . 3)
                                        (write . 3)
                                        (truncate . 2)
                                        (unlink . 1))
  "An alist of Fuse operation name/arity pairs supported by Elfuse.")

(defun elfuse-start (mountpath)
  "Start Elfuse using a given MOUNTPATH."
  (interactive "DElfuse mount path: ")
  (if (elfuse--dir-mountable-p mountpath)
      (let ((abspath (file-truename mountpath)))
	(elfuse--mount abspath)
        (add-hook 'kill-emacs-hook 'elfuse--stop))
    (message "Elfuse: %s does not exist or is not empty." mountpath)))

(defun elfuse-stop ()
  "Stop Elfuse."
  (interactive)
  (elfuse--stop)
  (remove-hook 'kill-emacs-hook 'elfuse--stop))

(define-error 'elfuse-op-error "Elfuse operation error")

(defmacro elfuse-define-op (opname arglist &rest body)
  "Define a Fuse operation OPNAME handler.
Apart from defining the function required by Elfuse the macro
also checks that the OPNAME is a supported Fuse operation and
there's a correct number of arguments in the ARGLIST. A list of
correct ops is defined in the `elfuse--supported-ops-alist'
variable.

Argument ARGLIST is a list of operation arguments.

Optional argument BODY is a body of the function that will handle
the operation."
  (declare (indent 2))
  (cond ((not (assq opname elfuse--supported-ops-alist))
         `(error "Operation '%s' not supported" ,(symbol-name opname)))
        ((not (= (alist-get opname elfuse--supported-ops-alist) (length arglist)))
         `(error "Operation '%s' requires %d arguments"
                 ,(symbol-name opname)
                 ,(alist-get opname elfuse--supported-ops-alist)))
        (t `(defun ,(intern (concat "elfuse--" (symbol-name opname) "-op"))
                ,arglist
              ,@body))))

(define-key special-event-map (kbd "<sigusr1>")
  (lambda ()
    (interactive)
    (elfuse--check-ops)))

(defun elfuse--dir-mountable-p (path)
  (and (file-exists-p path)
       ;; only . and ..
       (= (length (directory-files path)) 2)))

(provide 'elfuse)
